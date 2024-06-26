# AC Sensor
An ESP32-based device that measures the amount of water in the reservoir and triggers SMS warnings when above a certain threshold. 

![Project gif](assets/v2/project_vid.gif)


## Circuit
### Components
* ESP32 board
* 16x2 i2c LCD
* HX711 weighing module
* Small wire (touch sensor)
* Breadboard power supply module

### Connection
![Project diagram](assets/circuit_diagram.JPG)

### Calibration
Before getting the weight of objects, you first need to calibrate your load cell by obtaining the calibration factor. Run the file ```calibrate_scale.ino``` to calibrate the scale.
1. Prepare an object with a known weight (i.e. a glass of water with 300g).
2. Upload the calibration code to your ESP32.


### Description
* The HX711 weigh module (previously calibrated) weighs the AC water tank; 
* The weight is mapped: 200g -> 0% and 2000g -> 100%;
* A task running in parallel sends the mapped weight to ThingSpeak every 16 seconds;
* If the mapped weight exceeds 70 (%), an SMS is sent warning of the water tank level;
* An LCD shows the measured weight; its backlight stays on for 5 seconds when a touch sensor is activated.



