# HC138 SPI Multiplexer

The HC138 component allows you to use the 74HC138/CD4051B 3-to-8 line decoder/demultiplexer to control multiple SPI devices on a single SPI bus. This is particularly useful when you need to connect multiple SPI devices that share the same SPI bus (MOSI, MISO, CLK) but have different CS pins.

## Configuration

```yaml
# Example configuration
spi:
  id: spi0
  clk_pin: GPIO14
  mosi_pin: GPIO12
  miso_pin: GPIO13

hc138:
  id: multiplex0
  spi_id: spi0
  a0: GPIO23  # Address pin A0
  a1: GPIO22  # Address pin A1
  a2: GPIO21  # Address pin A2
  channels:
    - bus_id: multiplex0ch0  # First channel
      channel: 0
    - bus_id: multiplex0ch1  # Second channel
      channel: 1
```

### Configuration variables:

- **id** (*Required*, ID): The ID to use for this HC138 multiplexer.
- **spi_id** (*Required*, ID): The ID of the SPI bus to use.
- **a0** (*Required*, Pin): The GPIO pin connected to address pin A0 of the HC138.
- **a1** (*Required*, Pin): The GPIO pin connected to address pin A1 of the HC138.
- **a2** (*Required*, Pin): The GPIO pin connected to address pin A2 of the HC138.
- **channels** (*Required*, List): List of channels to use.
  - **bus_id** (*Required*, ID): The ID to use for this channel.
  - **channel** (*Required*, int): The channel number (0-7).

## Example with Multiple Devices

```yaml
spi:
  id: spi0
  clk_pin: GPIO14
  mosi_pin: GPIO12
  miso_pin: GPIO13

hc138:
  id: multiplex0
  spi_id: spi0
  a0: GPIO23
  a1: GPIO22
  a2: GPIO21
  channels:
    - bus_id: multiplex0ch0
      channel: 0
    - bus_id: multiplex0ch1
      channel: 1

# Example using MAX31865 temperature sensor on channel 0
sensor:
  - platform: max31865
    spi_id: multiplex0ch0
    name: "Temperature Sensor 1"
    cs_pin: GPIO15
    rtd_wires: 3
    reference_resistance: 3900 Ω
    rtd_nominal_resistance: 1000 Ω

  # Another MAX31865 on channel 1
  - platform: max31865
    spi_id: multiplex0ch1
    name: "Temperature Sensor 2"
    cs_pin: GPIO15
    rtd_wires: 3
    reference_resistance: 3900 Ω
    rtd_nominal_resistance: 1000 Ω
```

## Working Principle

The HC138 is a 3-to-8 line decoder/demultiplexer that allows you to control up to 8 different SPI devices using only 3 GPIO pins (A0, A1, A2). When you want to communicate with a specific device:

1. The component sets the address pins (A0, A1, A2) to select the desired channel (0-7)
2. The selected channel becomes active, allowing communication with the device on that channel
3. The SPI communication proceeds normally through the shared SPI bus

This setup allows you to connect up to 8 SPI devices while using only 3 additional GPIO pins for channel selection, instead of using a separate CS pin for each device.

## See Also

- [SPI Component](/components/spi/)
- [MAX31865 Component](/components/sensor/max31865/)
