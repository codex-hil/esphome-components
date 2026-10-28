# ZeroTier verification

Use Python from the pinned ESPHome 2026.9.0 Nix closure, not host pip.
From the repository root:

```sh
python tests/zerotier/test_schema.py
python tests/zerotier/test_source.py /path/to/pinned/ZeroTierOne
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -Icomponents/zerotier/idf/include tests/zerotier/frame_test.c -o /tmp/zt-frame-test
/tmp/zt-frame-test
cc -std=c11 -Wall -Wextra -Wno-unused-parameter -fsanitize=address,undefined \
  -Itests/zerotier/stubs -I/path/to/pinned/ZeroTierOne/include \
  tests/zerotier/nvs_test.c components/zerotier/idf/zt_nvs.c -o /tmp/zt-nvs-test
/tmp/zt-nvs-test
```

`test_source.py` checks exact revision, patch applicability (including upstream
CRLF files), repeatable cache use and protection of unowned directories.
The C tests exercise frame boundaries, unsupported EtherTypes/routes and NVS
fault injection. They are host tests, not malformed-packet HIL validation.

`hil_station2.py` is deliberately tied to the identified laboratory board and
its verified private original backup. It acquires both existing board leases,
verifies current flash before mutation, opens a bounded 720-second client
window and restores/verifies the first 16 MiB in `finally`. This covers both
OTA slots in the private test layout. Do not repurpose it for a different
board by removing identity checks. A host power failure still requires manual
recovery from backup; software `finally` cannot cover loss of station power.

`api_ota_client.py` runs on the independently authorized peer during that
window. It checks encrypted API, MAC, Node ID, virtual address, configured
state and increasing uptime over LAN and ZeroTier concurrently, uploads the
specified firmware through the ZeroTier address using the official ESPHome
OTA client, and repeats API checks after reboot. Its result must be copied to
`client-done.json` in the active private run directory before lease expiration.
Secrets, firmware and full serial logs stay outside this repository.

The OTA test uploads the same tested application; successful upload plus
uptime reset and subsequent encrypted API access prove the update/reboot path,
not a different application version. Power-loss OTA recovery is not tested.
