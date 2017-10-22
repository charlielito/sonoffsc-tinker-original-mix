load('api_aws.js');

load('api_config.js');

load('api_gpio.js');

load('api_mqtt.js');

load('api_net.js');

load('api_sys.js');

load('api_timer.js');

load('api_uart.js');



////////////////////////////// BeMo RS Config //////////////////////////////



let bemors_type = "Somfy";

let bemors_steps = 2; // steps 0 1

let shadow_version = -1;

////////////////////////////// BeMo RS Config //////////////////////////////





////////////////////////////// UART //////////////////////////////

let UART_number = 2;

UART.setConfig(UART_number, {

  baudRate: 9600,

  numDatabits: 8,

  parity:0,

  numStopBits:1,

  esp32: {

    gpio: {

      rx: 13,

      tx: 16

    }

  }

});





////////////////////////////// UART //////////////////////////////



let getInfo = function() {

  return JSON.stringify({total_ram: Sys.total_ram(), free_ram: Sys.free_ram()});

};



////////////////////////////// Tick //////////////////////////////

//let get_led_gpio_pin = ffi('int get_led_gpio_pin()');

let led = 2;



// Blink built-in LED every second

GPIO.set_mode(led, GPIO.MODE_OUTPUT);

Timer.set(30000 /* 1 sec */, true /* repeat */, function() {

//    print("Timer set");

//    let value = GPIO.toggle(led);

//    print(value ? 'Tick' : 'Tock', 'uptime:', Sys.uptime(), getInfo());

    print('Tick', 'uptime:', Sys.uptime(), getInfo());

}, null);



////////////////////////////// Tick //////////////////////////////



////////////////////////////// BUTTON //////////////////////////////

//let button = ffi('int get_button_gpio_pin()')();

//

//GPIO.set_button_handler(button, GPIO.PULL_UP, GPIO.INT_EDGE_NEG, 200, function() {

//    print('Button pressed');

//}, null);



////////////////////////////// BUTTON //////////////////////////////



////////////////////////////// MQTT //////////////////////////////



// actual reported shadow state

/*

 * Structure:

 * {

 *   unit("integer"): {

 *     channel("integer"): {

 *       position: U, D, S, +1, -1

 *     }

 *   }

 * }

 */

let internal_state_reported = {

    config: {

        bemors_type: bemors_type

        , bemors_steps: bemors_steps

    }

};



let isEmpty = function(object) {

    for(let _k in object) { return false; } return true;

};



/**

 * Generate one command for Somfy

 * @param {type} unit

 * @param {type} motor

 * @param {type} position

 * @returns {unresolved}

 */

let somfy_generate_command = function( unit, motor, position ) {

    /*

     * position: UP(U) DOWN(D) STOP(S), TILT_UP_1(+1), TILT_DOWN_1(-1)

     */

    let cmd = "";

    if( typeof unit === 'string' ) {

        if( unit.length === 1 ) {

            cmd += "0"+unit;

        } else if( unit.length === 2 ) {

            cmd += unit;

        } else {

            print("somfy_generate_command: unit string lenght is incorrect", unit.length);

            return null;

        }

    } else {

        print("somfy_generate_command: unit is not a string", unit);

        return null;

    }

    

    if( typeof motor === 'string' ) {

        if( motor.length === 1 ) {

            cmd += "0"+motor;

        } else if( motor.length === 2 ) {

            cmd += motor;

        } else {

            print("somfy_generate_command: motor string lenght is incorrect", motor.length);

            return null;

        }

    } else {

        print("somfy_generate_command: motor is not a string", motor);

        return null;

    }

    

    if( typeof position === 'string' ) {

        // valid positions are U: MAX UP, D: MAX DOWN, S: STOP, C: Custom, +1, -1

        if( position === 'U' || position === 'D' || position === 'S' ) {

            cmd += position;

        }

        if( position === 'C' ) {

            cmd += position;

            cmd += ';' + cmd; // send twice

        }

        if( position === '+1' || position === '1' ) {

            cmd += '1';

        }

        if( position === '-1' ) {

            cmd += ')';

        }

    } else {

        print("somfy_generate_command: position is not a string", position);

        return null;

    }

    return cmd;

};



/**

 * Loop throught desired shadow and generate serial commands

 * @param json desired

 * @returns Array

 */

let somfy_shade_loop = function( desired ) {

    let commands = [];

    let pos = 0;

    let count = 0;

    print("somfy_shade_loop", JSON.stringify(desired));

    for (let unit in desired) {

        print("somfy_shade_loop: unit", unit);

        for (let channel in desired[unit]) {

            if(!desired[unit][channel]["position"]) {

                // if not position key in object discard

                continue;

            }

            print("somfy_shade_loop: channel: ", unit, channel, desired[unit][channel]["position"]);

            let _c = somfy_generate_command(unit, channel, desired[unit][channel]["position"] );

            if( _c !== null ) {

                if(!commands[pos]) {

    //                print("inicializando comando");

                    commands[pos] = ''; // init

                }

                if(commands[pos].length>0) {

    //                print("concatenando comando");

                    commands[pos] = commands[pos] + ';' + _c;

                } else {

    //                print("primer comando");

                    commands[pos] = _c;

                }

                // update reported state variable only if a good command was generated

                if(!internal_state_reported[unit]) {

                    internal_state_reported[unit] = {};

                }

                internal_state_reported[unit][channel] = {'position': desired[unit][channel]['position']};

                print("Adding new reported state in global variable", internal_state_reported);

                count = count + 1;

                if(count >= 4) {

                    count = 0;

                    pos = pos + 1;

                    if((pos-1)>=0) {

                        // add to last command one wait, fith command is wait

                        commands[pos-1] = commands[pos-1] + ';W2';

                    }

                }

            } else {

                print("One command has been discharged");

            }

        }

    }

    return commands;

};



/*

 * Send and UART command to Somfy device

 * @cmd String

 */

let somfy_shade_send_cmd = function( cmd ) {

    print("UART send", JSON.stringify(cmd));

    UART.write(UART_number, cmd);

};



/*

 * We subscrite to update accepted shadow so we can receive all updates even on no delta

 */

MQTT.sub('$aws/things/' + Cfg.get('device.id') + '/shadow/update/accepted',function(conn,topic,message){

//    print("onMessage accepted update shadow", conn, topic);

    let shadow = JSON.parse (message);

//    print ('Got Message:', message);

    // turn on led

    GPIO.write(led, 1);

    if(shadow.version > shadow_version) {

        print("New shadow version than actual", shadow.version, shadow_version, message);

        shadow_version = shadow.version;

        if( shadow.state && shadow.state.desired ) {

            let cmds = somfy_shade_loop(shadow.state.desired);

            print("cmds", JSON.stringify(cmds));

            if(cmds!== null && cmds.length>0) {

                for(let k in cmds) {

                    somfy_shade_send_cmd(cmds[k]);

                }

            }

            print("updating reported state", JSON.stringify(internal_state_reported));

            AWS.Shadow.update(0, {reported: internal_state_reported});  // Report device state

        } else {

            print("Not desired state shadow update, ommiting", message);

        }

    }

    // turn off led

    GPIO.write(led, 0);

});



/**

 * Becasue setStateHandler delta and update only activates on true delta, we use this just

 * for connected state.

 */

AWS.Shadow.setStateHandler(function(data, event, reported, desired) {

    print("AWS.Shadow.setStateHandler:", event, AWS.Shadow.CONNECTED, AWS.Shadow.GET_ACCEPTED, AWS.Shadow.GET_REJECTED, AWS.Shadow.UPDATE_ACCEPTED, AWS.Shadow.UPDATE_REJECTED, AWS.Shadow.UPDATE_DELTA);

    let changed = false;

    if (event === AWS.Shadow.CONNECTED) {

        print("AWS.Shadow.setStateHandler: CONNECTED");

        // We report initial state as soon as we can read the shade real status

        AWS.Shadow.update(0, {reported: internal_state_reported});  // Report device state

        print(JSON.stringify(reported), JSON.stringify(desired), JSON.stringify(internal_state_reported));

    }

}, null);



////////////////////////////// MQTT //////////////////////////////


