
#ifndef EVENTS_H
#define EVENTS_H

void robovacStateEvent(TimerInformation *Sender) {
    unsigned long currentTime = millis();

    handleActionState(currentTime);
}

void pollRxEvent(TimerInformation *Sender) {
    boolean CRCGood = false;
    uint8_t buffLen = sizeof(message_t);
    uint8_t *messageBuff = (uint8_t *) &message;
    unsigned long currentTime = millis();

    signalStrength = analogRead(signalStrengthPin);
    if (vw_have_message()) {
        digitalWrite(statusLEDPin, HIGH);
        CRCGood = vw_get_message(messageBuff, &buffLen);
        if ( (CRCGood == false) || (validMessage(&message) == false)
                                || (findNode(message.node_id) == NULL) ) {
                blankMessage = true;
                PRINTMESSAGE(millis(), message, signalStrength);
                if (CRCGood == false) {
                    D("Bad CRC ");
                }
                if (validMessage(&message) == false) {
                    D("Invalid message ");
                }
                if (findNode(message.node_id) == NULL) {
                    D("Invalid node_id ");
                    D(message.node_id)
                }
                D("\n");
        } else { // Good message
            blankMessage = false;
            lastReception = currentTime;
        }
    }
}

void statusEvent(TimerInformation *Sender) {
    PRINTMESSAGE(millis(), message, signalStrength);
    digitalWrite(statusLEDPin, LOW);
}

void lcdEvent(TimerInformation *Sender) {
    unsigned long currentTime = millis();
    uint8_t new_buttons=0;

    new_buttons = lcd.readButtons();
    if (new_buttons != lcdButtons) {
        lastButtonChange = currentTime;
        lcdButtons = new_buttons;
    }
    handleLCDState(currentTime);
}

#endif // EVENTS_H
