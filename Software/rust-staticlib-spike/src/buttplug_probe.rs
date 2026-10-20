//! Instantiates the unmodified upstream device catalog and server. No
//! communication manager is registered: the Arduino NimBLE host remains the
//! only BLE stack, and a later FFI transport would plug in here.

use std::{sync::Arc, time::Instant};

use buttplug_server::{
    ButtplugServer, ButtplugServerBuilder,
    device::{ServerDeviceManagerBuilder, get_default_protocol_map},
};
use buttplug_server_device_config::{ProtocolCommunicationSpecifier, load_protocol_configs};

use crate::log_memory;

const UPSTREAM_BUTTPLUG_REVISION: &str = "9571b3db42ee2d7b3342ab9d40eb5c9e45679444";

pub async fn start() -> Option<ButtplugServer> {
    println!("[rust] Buttplug revision {UPSTREAM_BUTTPLUG_REVISION}");
    let loading = Instant::now();
    let configuration = match load_protocol_configs(&None, &None, false)
        .and_then(|mut builder| builder.finish())
    {
        Ok(configuration) => Arc::new(configuration),
        Err(error) => {
            println!("[rust] could not load the upstream device catalog: {error}");
            return None;
        }
    };
    println!(
        "[rust] device catalog loaded in {} ms",
        loading.elapsed().as_millis()
    );
    log_memory("device catalog loaded");

    let factories = get_default_protocol_map();
    let ble_protocols = configuration
        .base_communication_specifiers()
        .iter()
        .filter(|(_, specifiers)| {
            specifiers
                .iter()
                .any(|specifier| matches!(specifier, ProtocolCommunicationSpecifier::BluetoothLE(_)))
        })
        .filter(|(protocol, _)| factories.contains_key(protocol.as_str()))
        .count();
    println!(
        "[rust] catalog: {} protocol factories, {} factory-backed BLE protocols",
        factories.len(),
        ble_protocols
    );

    let device_manager = match ServerDeviceManagerBuilder::new_with_arc(configuration).finish() {
        Ok(device_manager) => device_manager,
        Err(error) => {
            println!("[rust] could not build the device manager: {error}");
            return None;
        }
    };
    match ButtplugServerBuilder::new(device_manager).finish() {
        Ok(server) => {
            log_memory("server built");
            Some(server)
        }
        Err(error) => {
            println!("[rust] could not build the Buttplug server: {error}");
            None
        }
    }
}
