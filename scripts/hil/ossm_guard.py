"""OSSM motor interlock: no OSSM firmware runs unless the RS-485 guard protects it.

The independent guard (C:\\rad-hil\\rs485-guard, host-only) samples the 57AIM30
motor over RS-485 and owns the verified stop. Homing needs the motor handed back
to the OSSM, where the guard enforces its collision-only contact limits. A
latched guard means a fault awaits human review, so this harness never clears
it or re-arms the motor: the run fails before anything is written.
"""
from __future__ import annotations

from datetime import datetime, timezone
import json
from pathlib import Path
import threading
import time

MAX_STATUS_AGE_S = 5.0
MIN_RATE_HZ = 30.0
MAX_SAMPLE_AGE_MS = 500.0


def read_status(path):
    status = json.loads(Path(path).read_text(encoding='utf-8-sig'))
    written = datetime.fromisoformat(status['time'])
    status['status_age_s'] = (datetime.now(timezone.utc) - written).total_seconds()
    return status


def unsafe_reason(status):
    """Return why the guard cannot protect motion, or None when it can."""
    rail = status.get('rail') or {}
    if status['status_age_s'] > MAX_STATUS_AGE_S:
        return f"guard status is {status['status_age_s']:.0f} s old; the guard is not running"
    if not status.get('streamConnected'):
        return 'guard is not receiving bench telemetry'
    if (status.get('rateHz') or 0) < MIN_RATE_HZ:
        return f"RS-485 telemetry is {status.get('rateHz') or 0:.0f} Hz, below {MIN_RATE_HZ:.0f} Hz"
    if status.get('sampleAgeMs') is None or status['sampleAgeMs'] > MAX_SAMPLE_AGE_MS:
        return 'RS-485 telemetry is stale'
    if status.get('activeFault'):
        return f"guard fault: {status['activeFault']}"
    if not any(r.get('stopVerified') for r in status.get('stopResults') or []):
        return 'the guard has not verified its stop path'
    if rail.get('critical'):
        return 'the guard has latched the motor off; review the fault and hand the motor back before testing'
    if status.get('mode') != 'collision-only' or not (status.get('handback') or {}).get('verified'):
        return 'the motor has not been verifiably handed back to the OSSM under the guard'
    if (status.get('alerts') or {}) and not status.get('safe'):
        return 'guard alerts: ' + ', '.join(sorted(status['alerts']))
    return None


class GuardWatch:
    """Preflight plus continuous checks while an OSSM run is in progress."""

    def __init__(self, path, output):
        self.path = Path(path)
        self.output = Path(output)
        self.failure = None
        self.record = dict(schema=1, preflight=None, motion=None, before=None, after=None,
                           faults=[], contacts=[])
        self._stop = threading.Event()
        self._thread = None
        self._faults = None

    def require(self):
        try:
            status = read_status(self.path)
        except (OSError, ValueError, KeyError) as error:
            self.record['preflight'] = f'guard status unavailable: {error}'
            raise RuntimeError('RS-485 safety guard is not running; refusing to flash the OSSM') from error
        self.record['before'] = status
        reason = unsafe_reason(status)
        self.record['preflight'] = reason or (
            f"guard running at {status['rateHz']:.0f} Hz with a verified stop path")
        if reason:
            raise RuntimeError('RS-485 safety guard: ' + reason)
        self.record['motion'] = 'handed back to the OSSM; guard enforcing collision-only limits'
        self._faults = status.get('faultCount', 0)
        self._thread = threading.Thread(target=self._watch, daemon=True)
        self._thread.start()

    def _watch(self):
        while not self._stop.wait(1.0):
            try:
                status = read_status(self.path)
            except (OSError, ValueError, KeyError) as error:
                self._fail(f'guard status unreadable: {error}')
                continue
            for contact in status.get('contacts') or []:
                if len(self.record['contacts']) < 10 and contact not in self.record['contacts']:
                    self.record['contacts'].append(contact)
            if status.get('faultCount', 0) != self._faults or status.get('activeFault'):
                self._faults = status.get('faultCount', 0)
                self.record['faults'].append(dict(time=status.get('time'), reason=status.get('activeFault')))
                self._fail(f"guard stopped the motor: {status.get('activeFault') or 'fault recorded'}")
            elif status['status_age_s'] > MAX_STATUS_AGE_S:
                self._fail('guard stopped reporting during the run')
            elif (status.get('rail') or {}).get('critical'):
                self._fail('guard latched the motor off during the run')

    def _fail(self, reason):
        if not self.failure:
            self.failure = 'RS-485 safety guard: ' + reason

    def check(self):
        if self.failure:
            raise RuntimeError(self.failure)

    def close(self):
        self._stop.set()
        if self._thread:
            self._thread.join(timeout=5)
        try:
            self.record['after'] = read_status(self.path)
        except (OSError, ValueError, KeyError) as error:
            self.record['after'] = dict(error=str(error))
        self.output.mkdir(parents=True, exist_ok=True)
        (self.output / 'rs485-guard.json').write_text(json.dumps(self.record, indent=2, default=str) + '\n',
                                                      encoding='utf-8')
