#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../firmware/esp-idf/main/controller.c"

static uint32_t now_ms;
static bool estop, estop_edge, hardware_ok = true, tx_blocked;
static int adc_raw = 4095, angles[40], pulses[40], levels[40];
static unsigned char rx[20000];
static size_t rx_head, rx_tail;
static unsigned checks;
#define CHECK(x) do { ++checks; if (!(x)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); exit(1); } } while (0)
bool board_init(void) { return true; }
bool board_is_ready(void) { return hardware_ok; }
uint32_t board_millis(void) { return now_ms; }
bool board_estop_pressed(void) { bool edge = estop_edge; estop_edge = false; return estop || edge; }
int board_adc_raw(void) { return adc_raw; }
void board_gpio_write(unsigned pin, int value) { levels[pin] = value; }
void board_angle(unsigned pin, unsigned value) { angles[pin] = (int)value; }
void board_pulse(unsigned pin, unsigned value) { pulses[pin] = (int)value; }
size_t board_rx_available(void) { return rx_tail - rx_head; }
int board_rx_read(void) { return rx_head < rx_tail ? rx[rx_head++] : -1; }
void board_tx_try(const char* data, size_t length) { (void)data; (void)length; if (tx_blocked) return; }
static void reset(void) {
    now_ms = 0; estop = estop_edge = false; hardware_ok = true; tx_blocked = false; adc_raw = 4095;
    rx_head = rx_tail = 0; memset(angles,0,sizeof(angles)); memset(pulses,0,sizeof(pulses));
    vehicle_init();
}
static void poll(void) { vehicle_poll(); ++now_ms; }
static void send(const char* text) {
    size_t length = strlen(text);
    CHECK(rx_tail + length <= sizeof(rx));
    memcpy(rx+rx_tail,text,length); rx_tail += length;
    while (board_rx_available()) poll();
    rx_head = rx_tail = 0;
}
static void safe(void) {
    CHECK(state == STOPPED); CHECK(angles[18] == 90 && angles[19] == 0);
    CHECK(pulses[23] == 1000 && pulses[26] == 1000);
    CHECK(levels[32] == 1 && levels[25] == 0 && levels[33] == 0);
}
static void run_serial(void) { send("ARM\n"); CHECK(state == ARMED); send("RUN\n"); CHECK(state == RUNNING); }
int main(void) {
    reset(); safe(); send("RUN\nGAS 100\nESC 100\nSTEER 45\n"); safe();
    run_serial(); CHECK(pulses[26] == 1500 && levels[33] == 1);
    send("STEER 45\nGAS 100\nESC 100\n");
    CHECK(angles[18] == 45 && angles[19] == 135 && pulses[23] == 1600);
    send("STEER 135\nGAS 50\nESC 50\n");
    CHECK(angles[18] == 135 && angles[19] == 67 && pulses[23] == 1300);
    const uint32_t good_time = lastValidCommandMs;
    const char* invalid[] = {"GAS abc\n","GAS 50xyz\n","ESC -1\n","ESC 101\n","GAS 1.5\n",
      "STEER 44\n","STEER 136\n","ESC 1 2\n","ESC 999999999\n","GAS\n","PING junk\n",
      "BAT junk\n","STOP junk\n","ARM\n","RUN\n","\n","   \n"};
    for (size_t i=0;i<sizeof(invalid)/sizeof(invalid[0]);++i) {
        send(invalid[i]); CHECK(lastValidCommandMs == good_time);
        CHECK(angles[18] == 135 && angles[19] == 67 && pulses[23] == 1300);
    }
    now_ms=good_time+TIMEOUT_MS; poll(); safe(); send("RUN\n"); safe();
    puts("PASS C FSM, ranges, strict parser and timeout");

    const char* keepalives[]={"PING\n","BAT\n","STEER 90\n","GAS 0\n","ESC 0\n"};
    for(size_t i=0;i<5;++i) {
        reset(); run_serial(); now_ms=lastValidCommandMs+TIMEOUT_MS-1; send(keepalives[i]);
        uint32_t accepted=lastValidCommandMs; now_ms=accepted+TIMEOUT_MS-1; poll(); CHECK(state==RUNNING);
        now_ms=accepted+TIMEOUT_MS; poll(); safe();
    }
    reset(); run_serial(); now_ms=lastValidCommandMs+TIMEOUT_MS; send("PING\nARM\nRUN\n"); safe();
    reset(); run_serial(); estop=true; poll(); safe(); send("ARM\nRUN\n"); safe();
    estop=false; send("RUN\n"); safe(); run_serial();
    reset(); estop=true; vehicle_init(); send("ARM\nRUN\n"); safe();
    reset(); estop_edge=true; CHECK(!vehicle_execute("ARM",11,1)); safe();
    CHECK(vehicle_execute("ARM",11,2)); // A fresh operator request after release.
    CHECK(vehicle_execute("RUN",11,3)); estop_edge=true; poll(); safe();
    reset(); run_serial(); adc_raw=3314; now_ms+=20; poll(); safe();
    CHECK(strcmp(stopReason,"LOW_BATTERY")==0); send("ARM\nRUN\n"); safe();
    adc_raw=4095; now_ms+=20; poll(); safe(); run_serial();
    reset(); adc_raw=3315; vehicle_init(); run_serial(); CHECK(batteryMv==10200);
    reset(); send("ARM\n"); adc_raw=0; send("RUN\n"); safe();
    reset(); run_serial(); adc_raw=-1; now_ms+=20; poll(); safe(); CHECK(strcmp(stopReason,"ADC_READ_FAILED")==0);
    reset(); run_serial(); hardware_ok=false; poll(); safe(); CHECK(strcmp(stopReason,"HARDWARE_FAULT")==0);
    puts("PASS C deadline, E-Stop, low battery and ADC/hardware faults");

    reset(); run_serial(); send("ESC 100"); CHECK(pulses[23]==1000); estop=true; poll(); safe();
    estop=false; send("\nARM\nRUN\n"); CHECK(state==RUNNING);
    char oversized[240]; memset(oversized,'X',200); strcpy(oversized+200,"ESC 100\n"); send(oversized); CHECK(pulses[23]==1000);
    send(" eSc\t50 \r\n"); CHECK(pulses[23]==1300);
    rx[0]='E'; rx[1]='S'; rx[2]='C'; rx[3]=' '; rx[4]='1'; rx[5]=0; rx[6]='\n'; rx_tail=7; rx_head=0;
    poll(); CHECK(pulses[23]==1300); rx_tail=rx_head=0;
    send("STOP\nARM\nRUN\n"); safe();
    reset(); run_serial(); tx_blocked=true;
    memset(rx,'X',10000); rx_head=0; rx_tail=10000; poll(); CHECK(rx_head<=32);
    estop=true; poll(); safe();
    reset(); run_serial(); tx_blocked=true;
    for(unsigned i=0;i<5100;++i) { send("noise\n"); }
    safe();
    reset(); now_ms=UINT32_MAX-200; run_serial(); uint32_t wrap_time=lastValidCommandMs;
    now_ms=wrap_time+TIMEOUT_MS-1; poll(); CHECK(state==RUNNING); now_ms=wrap_time+TIMEOUT_MS; poll(); safe();
    puts("PASS C partial/binary/oversized lines, floods and clock wrap");

    reset(); CHECK(vehicle_execute("ARM",11,1)); CHECK(vehicle_execute("RUN",11,2));
    CHECK(vehicle_execute("GAS 50",11,3)); CHECK(angles[19]==67);
    uint32_t last_phone=lastValidCommandMs; now_ms+=100;
    CHECK(!vehicle_execute("PING",11,3)); CHECK(!vehicle_execute("ESC 100",22,4));
    send("PING\nESC 100\nBAT\n"); CHECK(lastValidCommandMs==last_phone && pulses[23]==1000);
    CHECK(vehicle_execute("PING",11,4)); CHECK(lastValidCommandMs==now_ms);
    CHECK(vehicle_execute("STOP",22,1)); safe(); CHECK(vehicle_status().owner==0);
    CHECK(!vehicle_execute("RUN",11,5)); safe();
    CHECK(vehicle_execute("ARM",22,6)); CHECK(vehicle_execute("RUN",22,7));
    send("STOP\n"); safe(); run_serial();
    CHECK(!vehicle_execute("PING",22,8));
    CHECK(!vehicle_execute("ESC 100\n",22,9));
    vehicle_stop("PHONE_DISCONNECTED"); safe();
    reset(); CHECK(vehicle_execute("ARM",33,1)); CHECK(vehicle_execute("RUN",33,2));
    now_ms=lastValidCommandMs+TIMEOUT_MS; CHECK(!vehicle_execute("PING",33,3)); safe();
    puts("PASS C single-owner control, session isolation and replay rejection");

    CHECK(vehicle_angle_us(0)==500 && vehicle_angle_us(90)==1500 && vehicle_angle_us(135)==2000);
    CHECK(vehicle_pulse_duty(1000)==3277 && vehicle_pulse_duty(1500)==4915 && vehicle_pulse_duty(1600)==5243);
    for(unsigned us=500;us<=2500;++us) {
        double actual=(double)vehicle_pulse_duty(us)*20000.0/65536.0;
        CHECK(actual>us-0.153 && actual<us+0.153);
    }
    printf("All native C controller tests passed (%u checks).\n",checks);
    return 0;
}
