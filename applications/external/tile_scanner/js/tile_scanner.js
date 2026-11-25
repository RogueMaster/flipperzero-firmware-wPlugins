
let eventLoop = require("event_loop");
let gui = require("gui");

let widget = require("gui/widget");

let serial = require("serial");
serial.setup("usart", 115200);
let isScanning = false;

let widgetElements = [
    { element: "text_scroll", x: 0, y: 0, w: 128, h: 50, text: ""},
    { element: "button", button: "right", text: "Start Scanning" },
    { element: "button", button: "left", text: "Clear" },
];

let views = {
    widget: widget.makeWith({}, widgetElements)
};

eventLoop.subscribe(gui.viewDispatcher.navigation, function (_sub, _) {
    eventLoop.stop();
});

eventLoop.subscribe(eventLoop.timer("periodic", 500), function(_sub, _item, count, eventLoop) {
    let data = serial.readln(10);
    
    if(data === undefined) {
        return;
    }
    
    let hasChanged = false;
    
    // print("Received( " + data.length.toString() + " ): " + data);
    
    if(data[0] === 'h'){
        let prev = isScanning;
        isScanning = data[1] === '1';
        
        hasChanged = prev !== isScanning;
        widgetElements[1].text = isScanning ? "Stop Scanning" : "Start Scanning";
    }else{
        hasChanged = true;
        widgetElements[0].text += data + "\n";
    }
    
    if(hasChanged){
        views.widget.setChildren(widgetElements);
    }
}, 1, eventLoop);

eventLoop.subscribe(views.widget.button, function (_sub, buttonEvent, gui, views) {
    if (buttonEvent.key === "right") {
        if(isScanning) {
            print("Stopping Scanner");
            serial.write("s");
        }else{
            print("Starting Scanner");
            serial.write("b");
        }
    }else if(buttonEvent.key === "left") {
        widgetElements[0].text = "";
        views.widget.setChildren(widgetElements);
    }
}, gui, views);

// run UI
gui.viewDispatcher.switchTo(views.widget);
eventLoop.run();

serial.write("s");
