#!/usr/bin/env sh

set -eu

core_sources="Modules/Networking/networking_message_transport.cpp
Modules/Networking/networking_secure_channel.cpp
Modules/Networking/networking_handshake.cpp
Modules/Networking/networking_replication_protocol.cpp
Modules/Networking/networking_replication_revision_tracker.cpp
Modules/Networking/networking_replication_apply_budget.cpp
Modules/Networking/networking_replication_client.cpp
Modules/Networking/networking_replication_retention_window.cpp"

for source_path in $core_sources; do
    if [ ! -f "$source_path" ]; then
        printf 'missing Networking core source: %s\n' "$source_path" >&2
        exit 1
    fi
    if grep -E -i 'openssl|NETWORKING_HAS_OPENSSL' "$source_path" \
        >/dev/null 2>&1; then
        printf 'OpenSSL dependency leaked into Networking core: %s\n' \
            "$source_path" >&2
        exit 1
    fi
done

crypto_dependency="$(grep -R -E -i 'Networking/|\.\./Networking' \
    Modules/Crypto --include='*.cpp' --include='*.hpp' || true)"
if [ -n "$crypto_dependency" ]; then
    printf '%s\n' 'Networking dependency leaked into Crypto module:' >&2
    printf '%s\n' "$crypto_dependency" >&2
    exit 1
fi

printf '%s\n' 'Networking/Crypto module boundary check passed'
