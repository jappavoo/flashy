import CoreBluetooth
import Foundation
import Darwin

let VERBOSE = false

extension StringProtocol {
    var asciiValues: [UInt8] { compactMap(\.asciiValue) }
}

class NUSPeripheral: NSObject {
        /// MARK: - LED services and charcteristics Identifiers
    public static let uartServiceUUID     = CBUUID.init(string: "6e400001-b5a3-f393-e0a9-e50e24dcca9e")
    public static let RxCharacteristicUUID = CBUUID.init(string: "6e400003-b5a3-f393-e0a9-e50e24dcca9e")
    public static let TxCharacteristicUUID = CBUUID.init(string: "6e400002-b5a3-f393-e0a9-e50e24dcca9e")
    public static let TxMaxCharacters = 20
}



class BLEUart: NSObject, CBPeripheralDelegate, CBCentralManagerDelegate {
  private var centralManager: CBCentralManager!
  private var blePeripheral: CBPeripheral!
  private var uartService: CBService?
  private var rxCharacteristic: CBCharacteristic?
  private var txCharacteristic: CBCharacteristic?
  private var txWriteType = CBCharacteristicWriteType.withResponse

  override init() {
    super.init();
    centralManager = CBCentralManager(delegate: self, queue: nil)
    if  VERBOSE { print("BLEController: init() finished"); }
  }
     
  func centralManagerDidUpdateState(_ central: CBCentralManager){
      if VERBOSE { print("Central state update") }
      switch central.state{
      case .poweredOff:
          if  VERBOSE { print("Power Off") }
      case .poweredOn:
          if  VERBOSE { print("Power On") }
      case .resetting:
          if  VERBOSE { print("Resetting")} 
      case .unsupported:
          if  VERBOSE { print("Unsupported") } // always in this state
      default: break
      }
      if central.state != .poweredOn {
          if  VERBOSE { print("Central is not powered on") }
      } else {
          if  VERBOSE { print("Starting scan for", NUSPeripheral.uartServiceUUID); }
          centralManager.scanForPeripherals(withServices: [NUSPeripheral.uartServiceUUID],
                                            options: [CBCentralManagerScanOptionAllowDuplicatesKey : false])
          //withServices: [NUSPeripheral.uartServiceUUID],
          //options: [CBCentralManagerScanOptionAllowDuplicatesKey : true])
      }
  }
  
  // Handles the result of the scan
  func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral,
                      advertisementData: [String : Any], rssi RSSI: NSNumber) {
      if  VERBOSE { print("centralManager(): Found! \(NUSPeripheral.uartServiceUUID) -  RSSI: \(RSSI) adData: \(advertisementData)") }
      // We've found it so stop scan
      self.centralManager.stopScan()
      // Copy the peripheral instance
      self.blePeripheral = peripheral
      self.blePeripheral.delegate = self

      // Connect!
      if  VERBOSE { print("Starting connect...") }
      self.centralManager.connect(self.blePeripheral, options: nil)
  }

  func centralManager(_ central: CBCentralManager, didFailToConnect peripheral: CBPeripheral, error: Error?) {
      // Handle error
      if  VERBOSE { print("centralManger: Connect didFailToConnect") }
  }

   func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
      if  VERBOSE { print("centralManager: didConnectPeripheral") }
      if (peripheral == blePeripheral) {
          if  VERBOSE { print("Connected to blePeripheral ... starting service discovery"); }
          peripheral.discoverServices([NUSPeripheral.uartServiceUUID])
      }
  }

  func centralManager(_ central: CBCentralManager, didDisconnect peripheral: CBPeripheral, error: Error?) {
      if  VERBOSE { print("centeralManager: DISCONNECTED"); }
      blePeripheral.delegate = nil
      blePeripheral = nil
      uartService = nil
      rxCharacteristic = nil
      txCharacteristic = nil
  }
  
  func peripheral(_ peripheral: CBPeripheral, didModifyServices invalidatedServices: [CBService]) {
      if  VERBOSE { print("peripheral: didModifyServices") }
  }

  // Handles discovery event
  func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
      if  VERBOSE { print("peripheral - didDiscoverServices") }
     if let services = peripheral.services {
         for service in services {
             if  VERBOSE { print(service.uuid) }
           if service.uuid == NUSPeripheral.uartServiceUUID {
               if  VERBOSE { print("NUS: UART service found : Starting discovery of characteristics...") }
             //         //Now kick off discovery of characteristics
             uartService = service
             peripheral.discoverCharacteristics([NUSPeripheral.RxCharacteristicUUID,
                                                 NUSPeripheral.TxCharacteristicUUID],
                                                for: service)
             return
           }
         }
     }
  }
  // MARK: - Strings
  func hexString(data: NSData) -> String {
      var bytes = [UInt8](repeating: 0, count: data.length)
    data.getBytes(&bytes, length: data.length)
    
    let hexString = NSMutableString()
    for byte in bytes {
        hexString.appendFormat("%02X ", UInt(byte))
    }
    
    return NSString(string: hexString) as String
  }

  func writeUart(data: Data) {
      self.blePeripheral?.writeValue(data, for: txCharacteristic!, type: .withResponse)
  }

  // called to ack successfull write 
  func peripheral(_ peripheral: CBPeripheral, didWriteValueFor characteristic: CBCharacteristic, error: Error?) {
      if let error = error {
          // Handle error
          if  VERBOSE { print("peripheral: didWriteValueFor Characteristic: ERROR \(error)") }
          return
      }
      // Successfully wrote value to characteristic
      if  VERBOSE { print("Wrote value for \(characteristic.uuid)") }
  }
  
  func uartIsReady() {
    // Called when peripheral is ready to send write without response again.
      // Write some value to some target characteristic.
      if  VERBOSE { print("pripheralIsReady") }
      writeUart(data:Data(bytes:"F\n".asciiValues, count:2))
  }

  func readStdin() {
      var data =  [UInt8](repeating:0, count:255)
      let n = read(FileHandle.standardInput.fileDescriptor, &data, 255);
      if  VERBOSE {
          print("stdin: \(n)"); 
          write(FileHandle.standardOutput.fileDescriptor, &data, n)
      }
      writeUart(data:Data(bytes:data, count:n))
      if  VERBOSE { print("Wrote: \(n)") }
      DispatchQueue.global().async { self.readStdin() }
  }
  
  // Handling discovery of characteristics
  func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: Error?) {
      
      if let characteristics = service.characteristics {
          for characteristic in characteristics {
              if  VERBOSE { print("peripheral: didDiscoverCharacteristicsFor: \(characteristic.uuid)") }
              if characteristic.uuid == NUSPeripheral.RxCharacteristicUUID {
                  if  VERBOSE { print("UART RX characteristic found: properties: \(characteristic.properties)") }
                  rxCharacteristic = characteristic
                  peripheral.setNotifyValue(true, for: characteristic)
                  // peripheral.discoverDescriptors(for: characteristic)
              } else if characteristic.uuid == NUSPeripheral.TxCharacteristicUUID {
                  if  VERBOSE { print("UART TX characteristic found : properties: \(characteristic.properties)") }
                  txCharacteristic = characteristic
                  peripheral.setNotifyValue(true, for: characteristic)
                  if  VERBOSE { print("tx len: withReponse=\(peripheral.maximumWriteValueLength(for:.withResponse)) withoutReponse=\(peripheral.maximumWriteValueLength(for:.withoutResponse))") }
                  // peripheral.discoverDescriptors(for: characteristic)
                  //if  VERBOSE { print(characteristic.properties.writeWithoutResponse); }
                  //txWriteType = characteristic.properties.contains(.writeWithoutResponse) ? .withoutResponse:.withResponse
                  //                  let  bytes =  NSData(bytes:"F\n".asciiValues, length:2)
                  // writeUart(value: Data(bytes:"F\n".asciiValues, count:2))
                  uartIsReady()
                  DispatchQueue.global().async { self.readStdin() }
              } 
          }
      }
  }
  
 
  
  func peripheral(_ peripheral: CBPeripheral, didDiscoverDescriptorsFor characteristic: CBCharacteristic, error: Error?)
  {
      if  VERBOSE { print("peripheral: didDiscoverDescriptorFor uuid: \(characteristic.uuid) descriptors: \(String(describing:characteristic.descriptors))") }
  }
  
  func peripheral(_ peripheral: CBPeripheral, didUpdateNotificationStateFor characteristic: CBCharacteristic, error: Error?) {
      
      if  VERBOSE { print("peripheral: didUpdateNotificationStateForCharacteristic") }
  }

  func readUartValue() {
      self.blePeripheral?.readValue(for: rxCharacteristic!)
  }

// In CBPeripheralDelegate class/extension
  func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: Error?) {
    if let error = error {
        // Handle error
        if  VERBOSE { print("peripheral: didUpdateValueForCharacteristic: ERROR \(error)") }
        return 
    }
    
    guard var value = characteristic.value else {
        if  VERBOSE { print("peripheral: didUpdateValueForCharacteristic: NO Value?") }
        return
    }

    if characteristic == rxCharacteristic && characteristic.service == uartService {
        if  VERBOSE { print("peripheral: didUpdateValueForCharacteristic for RX Value: \(hexString(data:value as NSData))") }
        write(FileHandle.standardOutput.fileDescriptor, &value, value.count)
        print()
    } else {
        if  VERBOSE { print("peripheral: didUpdateValueForCharacteristic OTHER Value: \(hexString(data:value as NSData))") }
    }
  }
}

var bleUart: BLEUart!


func main() {
//    signal(SIGPIPE, SIG_IGN)
    if  VERBOSE { print("Hello, world!") }
    bleUart = BLEUart()
    if  VERBOSE { print("main done") }
}

// OSX Event loop
//let runloop = CFRunLoopGetCurrent()


//CFRunLoopPerformBlock(runloop, kCFRunLoopDefaultMode, nil)
//CFRunLoopPerformBlock(runloop, kCFRunLoopDefaultMode), ^{ 
//    dispatch_async(dispatch_queue_create("main", nil)) {
//        main()
//        CFRunLoopStop(runloop)
//    }
//}
DispatchQueue.main.async {
    main()
}


if  VERBOSE { print("Enqueued main function call");} 

CFRunLoopRun()

if  VERBOSE { print("CFRunLoopRun: exited"); }
