#!/usr/bin/env bash
# Allocate the next firmware version for a merge into main and commit it as
# rad-version-control[bot]. Identical in OSSM, LKBX, DTT and RADR.
#
# Run from the repository root. Inputs: DEVICE_TYPE, VERSION_HEADER and
# RELEASE_VERSION (repository-relative paths), PR_TITLE (patch/minor/major
# bump rules live in release_version.py) and FIRMWARE_PUBLISH_TOKEN.
set -euo pipefail

if [[ "$GITHUB_REF_NAME" != main ]]; then
  echo "::error::Firmware versions are allocated only for merges into main."
  exit 1
fi

SOURCE_VERSION="$(python "$RELEASE_VERSION" read --header "$VERSION_HEADER")"
BUMP_TYPE="$(python "$RELEASE_VERSION" bump-type --title "$PR_TITLE")"
RESPONSE="$(curl --fail --silent --show-error \
  --retry 5 \
  --retry-delay 2 \
  --retry-max-time 60 \
  --header "Authorization: Bearer $FIRMWARE_PUBLISH_TOKEN" \
  --header "Content-Type: application/json" \
  --data "$(jq -n \
    --arg track main \
    --arg deviceType "$DEVICE_TYPE" \
    --arg bump "$BUMP_TYPE" \
    --arg sourceVersion "$SOURCE_VERSION" \
    '{track: $track, deviceType: $deviceType, bump: $bump, sourceVersion: $sourceVersion}')" \
  "https://dashboard.researchanddesire.com/api/internal/firmware/v1/versions/next")"
NEXT_VERSION="$(jq -er '.nextVersion' <<< "$RESPONSE")"

python "$RELEASE_VERSION" write --header "$VERSION_HEADER" --version "$NEXT_VERSION" --version-json version.json
test "$(python "$RELEASE_VERSION" read --header "$VERSION_HEADER")" = "$NEXT_VERSION"
test "$(jq -r '[.major, .minor, .patch] | map(tostring) | join(".")' version.json)" = "$NEXT_VERSION"
UNEXPECTED="$(git diff --name-only | grep -Fvx -e "$VERSION_HEADER" -e version.json || true)"
if [[ -n "$UNEXPECTED" ]]; then
  echo "Unexpected versioning changes:"
  echo "$UNEXPECTED"
  exit 1
fi
git add -- "$VERSION_HEADER" version.json
if git diff --cached --quiet; then
  echo "Version allocation did not change the canonical files"
  exit 1
fi

git config user.name "rad-version-control[bot]"
git config user.email "rad-version-control[bot]@users.noreply.github.com"
git commit -m "AUTO: Version bump main $NEXT_VERSION"
git push origin HEAD:main

RELEASE_BUILD_SHA="$(git rev-parse HEAD)"
echo "RELEASE_BUILD_SHA=$RELEASE_BUILD_SHA" >> "$GITHUB_ENV"
{
  echo "release_build_sha=$RELEASE_BUILD_SHA"
  echo "version=$NEXT_VERSION"
  echo "bump=$BUMP_TYPE"
} >> "$GITHUB_OUTPUT"
