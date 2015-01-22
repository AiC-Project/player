# AMQP Messages Reference

## Sensor port mapping table

The "various sensors" list is available in the AOSP tree under
hardware/libhardware/include/hardware/sensors.h.

Port   | Sensor
---    | ---
22471  | Various sensors (accelerometer, luxmeter, gyroscope…)
22473  | Battery
22475  | GPS
6704   | GSM

## Protobuf serialization

[General Protobuf introduction](https://developers.google.com/protocol-buffers/)

The source protobuf file is on the vm side (external/aic/libaicd)

To generate protobuf sources:

  
    protoc -I="./libandroidincloud" --cpp_out="./libandroidincloud" ./libandroidincloud/sensors_packet.proto
    protoc -I="./libandroidincloud" --java_out="./libandroidincloud" ./libandroidincloud/sensors_packet.proto
  


All the following are part of a common ("sensors_packet") protobuf Message, except "recording".


### GPS Message


~~~~~~{.proto}

    message sensors_packet {
        .
        .
        .
        // on another port
        optional GPSPayload                     gps = 200;
        .
        .
        .
        message GPSPayload {

            enum GPSStatusType {
                DISABLED = 0;
                ENABLED  = 1;
            }

            optional GPSStatusType  status    = 1 [default = ENABLED];
            optional double         latitude  = 2;
            optional double         longitude = 3;
            optional double         altitude  = 4;
            optional double         bearing   = 5;
        }

    }
~~~~~~

### Sensors Message


~~~~~~{.proto}
    message sensors_packet {

        //on the same port
        optional SensorAccelerometerPayload     sensor_accelerometer     =  1;
        optional SensorMagnetometerPayload      sensor_magnetometer      =  2;
        optional SensorOrientationPayload       sensor_orientation       =  3;
        optional SensorGyroscopePayload         sensor_gyroscope         =  4;
        optional SensorGravityPayload           sensor_gravity           =  5;
        optional SensorLinearAccPayload         sensor_linear_acc        =  6;
        optional SensorRotVectorPayload         sensor_rot_vector        =  7;
        optional SensorTemperaturePayload       sensor_temperature       =  8; // XXX ambient_temperature vs device_temperature
        optional SensorProximityPayload         sensor_proximity         =  9;
        optional SensorLightPayload             sensor_light             = 10;
        optional SensorPressurePayload          sensor_pressure          = 11;
        optional SensorRelativeHumidityPayload  sensor_relative_humidity = 12;
        .
        .
        .
        message SensorAccelerometerPayload {
            optional double x = 1;
            optional double y = 2;
            optional double z = 3;
        }

        message SensorMagnetometerPayload {
            optional double x = 1;
            optional double y = 2;
            optional double z = 3;
        }

        message SensorOrientationPayload {
            optional double azimuth = 1;
            optional double pitch   = 2;
            optional double roll    = 3;
        }

        message SensorGyroscopePayload {
            optional double azimuth = 1;
            optional double pitch   = 2;
            optional double roll    = 3;
        }

        message SensorGravityPayload {
            optional double x = 1;
            optional double y = 2;
            optional double z = 3;
        }

        message SensorLinearAccPayload {
            optional double x = 1;
            optional double y = 2;
            optional double z = 3;
        }

        message SensorRotVectorPayload {
            optional uint32 size = 1;
            repeated double data = 2 [packed=true];
        }

        message SensorTemperaturePayload {
            optional double temperature = 1;
        }

        message SensorProximityPayload {
            optional double distance = 1;
        }

        message SensorLightPayload {
            optional double light = 1;
        }

        message SensorPressurePayload {
            optional double pressure = 1;
        }

        message SensorRelativeHumidityPayload {
            optional double relative_humidity = 1;
        }
        .
        .
        .
    }
~~~~~~



### Battery Message


~~~~~~{.proto}
    message sensors_packet {
        .
        .
        .
        // on another port
        optional BatteryPayload                 battery = 100;
        .
        .
        .
        message BatteryPayload {

            enum BatteryStatusType {
                CHARGING    = 0 ;
                DISCHARGING = 2 ;
                NOTCHARGING = 3 ;
                FULL        = 4 ;
                UNKNOWN     = 5 ;
            }

            optional uint64             battery_level   = 1;
            optional uint64             battery_full    = 2;
            optional BatteryStatusType  battery_status  = 3 [default = CHARGING];
            optional uint32             ac_online       = 4;
        }
        .
        .
        .
    }
~~~~~~

### GSM Message

~~~~~~{.proto}

    message sensors_packet {
        .
        .
        optional GSMPayload                     gsm  = 300;

        message GSMPayload {
            enum GSMActionType  {
                RECEIVE_CALL             = 0;
                ACCEPT_CALL              = 1;
                CANCEL_CALL              = 2;
                HOLD_CALL                = 3;
                RECEIVE_SMS              = 4;
                SET_SIGNAL               = 5;
                SET_NETWORK_TYPE         = 6;
                SET_NETWORK_REGISTRATION = 7;
            }
            optional GSMActionType action_type      = 1;
            optional string        phone_number     = 2;
            optional string        sms_text         = 3;
            optional int32         signal_strength  = 4;
            optional string        network          = 5;
            optional string        registration     = 6;
        }
        .
        .
    }
~~~~~~

### Recording Message

The proto file is in the player repo and the C files are generated with
protoc-c when running cmake.

~~~~~~{.proto}
    message recordingPayload {
        optional string recFilename     = 1;
        optional uint32 startStop       = 2;
    }
~~~~~~
