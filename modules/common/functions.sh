#!/system/bin/sh

extract_binary() {
    mkdir -p "$MODPATH/system/bin"
    
    if [ "$IS64BIT" = true ]; then
        unzip -j -o "$ZIPFILE" 'system/bin/arm64-v8a/pgovd' -d "$MODPATH/system/bin" >&2
    else
        unzip -j -o "$ZIPFILE" 'system/bin/armeabi-v7a/pgovd' -d "$MODPATH/system/bin" >&2
    fi
}

extract_services() {
    unzip -o "$ZIPFILE" 'service.sh' 'uninstall.sh' -d "$MODPATH" >&2
}

set_permissions() {
    set_perm_recursive "$MODPATH" 0 0 0755 0644
    set_perm "$MODPATH/system/bin/pgovd" 0 0 0755 u:object_r:system_file:s0
    set_perm "$MODPATH/service.sh" 0 0 0755 u:object_r:script_exec:s0
    set_perm "$MODPATH/uninstall.sh" 0 0 0755
}

cleanup() {
    rm -rf "$MODPATH/common"
}