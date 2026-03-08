import React, { useState, useEffect } from 'react';
import { View, Button, Text, StyleSheet, PermissionsAndroid, Platform } from 'react-native';
import BleManager from 'react-native-ble-manager';

const BleManagerModule = NativeModules.BleManager;
const bleManagerEmitter = new NativeEventEmitter(BleManagerModule);

const App = () => {
  const [isScanning, setIsScanning] = useState(false);
  const [isConnected, setIsConnected] = useState(false);
  const [arduinoDevice, setArduinoDevice] = useState(null);

  useEffect(() => {
    BleManager.start({ showAlert: false });

    if (Platform.OS === 'android' && Platform.Version >= 23) {
      PermissionsAndroid.check(PermissionsAndroid.PERMISSIONS.ACCESS_FINE_LOCATION).then((result) => {
        if (result) {
          console.log("Permission is OK");
        } else {
          PermissionsAndroid.request(PermissionsAndroid.PERMISSIONS.ACCESS_FINE_LOCATION).then((result) => {
            if (result) {
              console.log("User accept");
            } else {
              console.log("User refuse");
            }
          });
        }
      });
    }

    const handlerDiscover = bleManagerEmitter.addListener('BleManagerDiscoverPeripheral', handleDiscoverPeripheral);
    const handlerStop = bleManagerEmitter.addListener('BleManagerStopScan', handleStopScan);

    return (() => {
      handlerDiscover.remove();
      handlerStop.remove();
    });
  }, []);

  const handleDiscoverPeripheral = (peripheral) => {
    if (peripheral.name === 'HC-05') { 
      console.log('Arduino device found');
      setArduinoDevice(peripheral);
      connectToArduino(peripheral);
    }
  };

  const handleStopScan = () => {
    console.log('Scan stopped');
    setIsScanning(false);
  };

  const startScan = () => {
    if (!isScanning) {
      BleManager.scan([], 5, true).then(() => {
        console.log('Scanning...');
        setIsScanning(true);
      });
    }
  };

  const connectToArduino = (peripheral) => {
    if (peripheral) {
      BleManager.connect(peripheral.id).then(() => {
        console.log('Connected to ' + peripheral.id);
        setIsConnected(true);
      }).catch((error) => {
        console.log('Connection error', error);
      });
    }
  };

  const toggleLED = async () => {
    if (!isConnected) {
      console.log('Arduino not connected');
      return;
    }

    const newLedState = !isLedOn;
    setIsLedOn(newLedState);

    const command = newLedState ? '1' : '0';

    
    const serviceUUID = 'tbd';
    const characteristicUUID = 'tbd';

    BleManager.write(arduinoDevice.id, serviceUUID, characteristicUUID, command)
      .then(() => {
        console.log('Toggled LED');
      })
      .catch((error) => {
        console.log('Toggle LED failed', error);
      });
  };

  return (
    <View style={styles.container}>
      <Button title={isScanning ? "Scanning..." : "Scan for Arduino"} onPress={startScan} disabled={isScanning} />
      <Button title="Toggle LED" onPress={toggleLED} disabled={!isConnected} />
      <Text>{isConnected ? "Connected to Arduino" : "Not connected"}</Text>
    </View>
  );
};

const styles = StyleSheet.create({
  container: {
    flex: 1,
    justifyContent: 'center',
    alignItems: 'center',
    backgroundColor: '#F5FCFF',
  },
});

export default App;
