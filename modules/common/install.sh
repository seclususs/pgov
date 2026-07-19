#!/system/bin/sh

. "$MODPATH/common/functions.sh"

print_header

ui_print "- Extracting binaries..."
extract_binary

ui_print "- Extracting conf..."
extract_conf

ui_print "- Configuring service..."
extract_services

ui_print "- Setting permissions..."
set_permissions

ui_print "- Cleaning up..."
cleanup