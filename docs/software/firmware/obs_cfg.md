# OpenBikeSensor configuration

This file describes the configuration format of the OBS internal(!)
configuration file. Please note that even that the format seems well
documented, it is considered an internal format which might change at
any time. There might be interpretations possible in which case 
the current implementation defines the correct used. 

The configuration is prepared for extension to support configuration 
presets, where certain values of the "default" configuration can be
changed for a certain presets. The handle bar offsets are very likely
candidates for this. The Wi-Fi or Portal configuration is more likely 
not part of a presets.

When working with backups please note that the configuration file contains 
your Wi-Fi password, and the portal token in plain text. 
Both values are secrets that should not be disclosed, so keep the backup
at a save place and replace these values when sharing.

## cfg.obs

Use the annotated sample file as documentation, order of the keys is not
relevant put the order of the array content.

```json5
{
  // obs must be an array. It can contain possibly multiple entries, 
  // the 1st defines default values for all presets, and the default presets.
    "obs": [ 
        {
          // enables / disables bluetooth
            "bluetooth": true,
          // time window for overtake measurement confirmation
            "confirmationTimeSeconds": 4,
          // Bitfield display config, see DisplayOptions for details, 
          // as noted all subject to change!
            "displayConfig": 15,
          // PIN to be entered when accessing the OBS web interface.
          // a new one will be created when empty. The PIN is also displayed 
          // on the OBS display when needed.
            "httpPin": "12345678",
          // Textual name of the profile - waiting for features to come.
            "name": "default",
          // Name of the OBS, will be used for WiFo, Bluetooth and other places 
          // currently no way to change it, consists of OpenBikeSensor-<deviceId>  
            "obsName": "OpenBikeSensor-cafe",
          // The handle bar offset in cm, order right, left.  
            "offset": [
                31,
                32
            ],
          // Token used to authenticate and authorize at the portal.
            "portalToken": "thisIsAlsoSecret",
          // URL of the portal to be used
            "portalUrl": "https://portal.openbikesensor.org",
          // Array with privacy areas to be considered
            "privacyArea": [
                {
                  // latitude of the entered value, for documentation and display purpose only
                    "lat": 48.001,
                  // shifted latitude used for position filtering
                    "latT": 48.00105,
                  // longitude of the entered value, for documentation and display purpose only
                    "long": 9.001,
                  // shifted longitude used for position filtering
                    "longT": 9.001121,
                  // radius in meters around the position to be filtered
                    "radius": 101
                }
            ],
          // Bitfield for the privacy configuration see PrivacyOptions
            "privacyConfig": 10,
          // Active preset - always 0 as of today
            "selectedPreset": 0,
          // Password of your Wi-Fi where the OBS should log into in server mode
            "wifiPassword": "swordfish",
          // SSID of your Wi-Fi where the OBS should log into in server mode
            "wifiSsid": "obs-cloud"
        }
    ]
}
```

