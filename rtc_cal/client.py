#!/usr/bin/env python

import sys
import os
import time
import datetime
import serial

def flush_output(serialdev):
    serialdev.flush()

def block_until(serialdev, what=">>> "):
    flush_output(serialdev)
    serialbuf = ""
    while serialbuf.find(what) == -1:
        sys.stdout.flush()
        newbyte = serialdev.read(1)
        if newbyte != "":
            sys.stdout.write(newbyte)
            serialbuf += newbyte
    sys.stdout.flush()
    flush_output(serialdev)

block_until_prompt = block_until

def echo_test(serialdev):
    serialdev.write("echo\n")
    flush_output(serialdev)
    block_until(serialdev, " one byte: ")
    serialdev.write(chr(0xEF))
    flush_output(serialdev)
    block_until(serialdev, "0xEF")

def setDT(serialdv):
    serialdev.write("setDT\n")
    block_until(serialdev, "Enter date/time in 'yymmddhhmmss' format:")
    #  Sync'd time is 2 seconds in future, zero microseconds
    deadline = datetime.datetime.now() + datetime.timedelta(seconds=2)
    deadline.replace(microsecond=0)
    serialdev.write("%02d" % (deadline.year - 2000))
    serialdev.write("%02d" % deadline.month)
    serialdev.write("%02d" % deadline.day)
    serialdev.write("%02d" % deadline.hour)
    serialdev.write("%02d" % deadline.minute)
    serialdev.write("%02d\n" % deadline.second)
    block_until(serialdev, "---Enter to synchronize---")
    #  Sync. to one ms before two seconds from now
    deadline -= datetime.timedelta(microseconds=1000)
    now = datetime.datetime.now()
    while now < deadline:
        now = datetime.datetime.now()
    serialdev.write("\r\n")
    flush_output(serialdev)
    block_until_prompt(serialdev)

def time1M(serialdev):
    serialdev.write("time1M\n")
    block_until(serialdev, "RTC ticks")
    block_until(serialdev, "START")
    start = time.time()
    time.sleep(50.0)
    block_until(serialdev, "STOP!")
    end = time.time()
    block_until_prompt(serialdev)
    return (start, end)

def time1H(serialdev):
    serialdev.write("time1H\n")
    block_until(serialdev, "RTC ticks")
    block_until(serialdev, "START")
    start = time.time()
    time.sleep((60.0 * 60.0) - 10)
    block_until(serialdev, "STOP!")
    end = time.time()
    block_until_prompt(serialdev)
    return (start, end)

def time6H(serialdev):
    serialdev.write("time6H\n")
    block_until(serialdev, "RTC ticks")
    block_until(serialdev, "START")
    start = time.time()
    time.sleep((60.0 * 60.0 * 6.0) - 30)
    block_until(serialdev, "STOP!")
    end = time.time()
    block_until_prompt(serialdev)
    return (start, end)

def print_time_results(start, end, seconds):
    duration = (end - start) * 32768
    offset = duration - (seconds * 32768)
    print
    print "   Start:", start
    print "     end:", end
    print "duration:", duration, "32 kHz ticks"
    print "expected:", seconds * 32768
    print "    diff:", offset, "ticks"

def setOffset(serialdev, start, end, seconds):
    duration = (end - start) * 32768
    serialdev.write('setOffset\n')
    block_until(serialdev, "Enter expected ticks:")
    serialdev.write(str(seconds * 32768))
    serialdev.write('\n')
    block_until(serialdev, "Enter actual ticks:")
    serialdev.write(str(int(duration)))
    serialdev.write('\n')
    block_until_prompt(serialdev)

def enter_command_mode(serialdev):
    # DTR signals reset on Arduino
    serialdev.setDTR(True)
    serialdev.flushOutput()  # discard output buffer
    serialdev.flushInput()  # discard input buffer
    print "Waiting for device reset..."
    serialdev.setDTR(False)
    block_until(serialdev, "Setup()")
    time.sleep(0.1)
    serialdev.write('<<<\n')
    flush_output(serialdev)
    time.sleep(0.1)
    serialdev.write('<<<\n')
    flush_output(serialdev)
    time.sleep(0.1)
    serialdev.write('<<<\n')
    block_until_prompt(serialdev)
    echo_test(serialdev)
    block_until_prompt(serialdev)

def init_serialdev(filename):
    serialdev = serial.Serial(filename, 115200, timeout=6, writeTimeout=6)
    if serialdev.getCTS():
        enter_command_mode(serialdev)
        return serialdev
    else:
        raise IOError("CTS not ready %s" % filename)

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print "Pass serial device as first & only parameter"
        sys.exit(1)

    serialdev = init_serialdev(sys.argv[1])
    setDT(serialdev)
    serialdev.write('dump\n');
    block_until_prompt(serialdev)

    start, end = time1M(serialdev)
    print_time_results(start, end, 60)
    setOffset(serialdev, start, end, 60)
    setDT(serialdev)
    serialdev.write('dump\n');
    block_until_prompt(serialdev)

    start, end = time1H(serialdev)
    print_time_results(start, end, 60 * 60)
    setOffset(serialdev, start, end, 60 * 60)
    setDT(serialdev)
    serialdev.write('dump\n');
    block_until_prompt(serialdev)

    # was   hzOffset/1000: -1259
    start, end = time6H(serialdev)
    print_time_results(start, end, 60 * 60 * 6)
    setOffset(serialdev, start, end, 60 * 60 * 6)
    setDT(serialdev)
    serialdev.write('dump\n');
    block_until_prompt(serialdev)
