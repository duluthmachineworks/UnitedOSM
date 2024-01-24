curl --location 'https://api.notefile.net/v1/projects/app:XXXX/devices/dev:XXXX/notes/settingsUpdate.qi' \
--header 'Content-Type: application/json' \
--header 'Authorization: Bearer XXX' \
--data '{
  "body": {
    "wifi_enabled": false,
    "power_on": true,
    "timer_mode": true,
    "time_on_hour": 7,
    "time_on_min": 0,
    "time_off_hour": 18,
    "time_off_min": 0,
    "logging_interval": 5,
    "inbound_interval": 10,
    "outbound_interval": 10
  }
}
'