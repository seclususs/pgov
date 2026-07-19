#!/system/bin/sh

print_header() {
    local mod_id=$(sed -n "s/^id=//p" "$MODPATH/module.prop")
    local mod_name=$(sed -n "s/^name=//p" "$MODPATH/module.prop")
    local mod_version=$(sed -n "s/^version=//p" "$MODPATH/module.prop")
    local mod_versioncode=$(sed -n "s/^versionCode=//p" "$MODPATH/module.prop")
    local mod_author=$(sed -n "s/^author=//p" "$MODPATH/module.prop")
    local mod_description=$(sed -n "s/^description=//p" "$MODPATH/module.prop")
    
    ui_print "*******************************************"
    ui_print "  ID           : $mod_id"
    ui_print "  Name         : $mod_name"
    ui_print "  Version      : $mod_version"
    ui_print "  Version Code : $mod_versioncode"
    ui_print "  Author       : $mod_author"
    ui_print "  Description  : $mod_description"
    ui_print "*******************************************"
}

extract_binary() {
    mkdir -p "$MODPATH/system/bin"
    
    if [ "$IS64BIT" = true ]; then
        unzip -j -o "$ZIPFILE" 'system/bin/arm64-v8a/pgovd' -d "$MODPATH/system/bin" >&2
    else
        unzip -j -o "$ZIPFILE" 'system/bin/armeabi-v7a/pgovd' -d "$MODPATH/system/bin" >&2
    fi
}

extract_conf() {
    unzip -o "$ZIPFILE" 'system/etc/pgovd.conf' -d "$MODPATH" >&2
}

extract_services() {
    unzip -o "$ZIPFILE" 'service.sh' 'uninstall.sh' -d "$MODPATH" >&2
}

set_permissions() {
    set_perm_recursive "$MODPATH" 0 0 0755 0644
    set_perm "$MODPATH/system/bin/pgovd" 0 0 0755 u:object_r:system_file:s0
    set_perm "$MODPATH/system/etc/pgovd.conf" 0 0 0644
    set_perm "$MODPATH/service.sh" 0 0 0755 u:object_r:script_exec:s0
    set_perm "$MODPATH/uninstall.sh" 0 0 0755
}

cleanup() {
    rm -rf "$MODPATH/common"
}