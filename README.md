# Features
- LCD display displaying either indoor temperature (using an analogue sensor) or outdoor temperature (using a digital sensor)
  - Displays current temperature as well as minimum/maximum temperatures for the past hour, in Celsius
  - The display can be interacted with through either two physical buttons (on/off and indoor/outdoor) or an IR emitter such as a fan remote.
    - Temperature readings can be reset through the IR receiver (see *IR controls* below)
  - Times out after two minutes of inactivity
- Temperature readings can be stored using either 8 or 16 bits
  - 8 bits allow for more samples (3600/h as opposed to 1500/h for 16 bits), but at a more constrained temperature range (7.0-32.5 C)
- Motion sensor that triggers a buzzer
  - The buzzer only reacts to signals from the sensor that last longer than 100 ms
  - The buzzer emits one-second tones in three-second intervals while active

# Components
- 1x Arduino MEGA 2560
- 1x LED Display
- 1x potentiometer
- 2x buttons
- 1x IR receiver
- 1x digital temperature sensor (DS18b20) 
- 1x analog temperature sensor
- 1x PIR sensor
- 1x buzzer
- 1x IR emitter (e.g. ir remote from a sensor kit or an abs121h fan remote)

# Pins
| Pin | Used by | Purpose |
|:---:|:-------:|:-------:|
| A0  | Analog temp | Indoor temperature readings |
| 2   | LCD display | Register Select |
| 3   | LCD display | Enable |
| 4-7 | LCD display | Data pins d4-d7 |
| 8   | LCD display | Anode; control backlight |
| 9   | Button 1    | Turn on/off the display |
| 10  | Button 2    | Toggle display |
| 11  | IR receiver | Enable remote interaction with display |
| 12  | Digital temp | Outdoor temperature readings |
| 14  | Buzzer      | Sound |
| 15  | PIR         | Motion detection |

# IR controls
![Remotes_annotated](https://user-images.githubusercontent.com/23358980/214530630-e6d296d8-ce6e-440a-a0dd-80a621cbf306.jpg)

# Example
![Outside temperature](https://user-images.githubusercontent.com/23358980/214530750-12f8d039-a7c6-45ca-a621-bf92ad35773e.jpg)
