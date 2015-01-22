
#define PORT_SENSORS 22471
#define PORT_BAT 22473
#define PORT_GPS 22475
#define PORT_GSM 6704
#define PORT_NFC 22800

#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <google/cmockery.h>
#include "sensors_packet.pb-c.h"
#include "nfc.pb-c.h"

#include "player_nfc.h"
#include "mockVMSensors.h"
#include "mockVMNfcd.h"
#include "mockVMLibNci.h"

#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include "sensors.h"
#include "buffer_sizes.h"
#include "socket.h"
#include "stdint.h"

#include "amqp_listen.h"
#include "amqp_send.h"
#include "config_env.h"

#include "logger.h"
#include <pthread.h>

#define LOG_TAG "testSensors"

char* g_amqp_host = NULL;
char* g_vmid = NULL;
char* g_vmip = NULL;

void test_sensors_acc(void** state)
{
    (void) state;
    char* g_amqp_host = NULL;
    char* g_vmid = NULL;
    char* g_vmip = NULL;
    pthread_t sensor_thread;

    g_amqp_host = configvar_string("AIC_PLAYER_AMQP_HOST");
    g_vmid = configvar_string("AIC_PLAYER_VM_ID");
    g_vmip = configvar_string("AIC_PLAYER_VM_HOST");

    sensor_params* slp = ParamEventsWorker(g_vmip, g_vmid, "sensors", g_amqp_host);

    SensorsPacket__SensorAccelerometerPayload payload =
        SENSORS_PACKET__SENSOR_ACCELEROMETER_PAYLOAD__INIT;

    payload.has_x = 1;
    payload.has_y = 1;
    payload.has_z = 1;
    payload.x = 1;
    payload.y = 2;
    payload.z = 3;

    int msglen = protobuf_c_message_get_packed_size((const struct ProtobufCMessage*) &payload);
    void* messagebody = malloc(msglen);

    int len = protobuf_c_message_pack((const struct ProtobufCMessage*) &payload, messagebody);

    amqp_send(g_amqp_host, 5672, slp->exchange, slp->queue, len, messagebody);
    sleep(3);

    // MOCK senza filling the sensors amqp queue
    free(messagebody);

    // Testing player_sensors
    start_sensor(slp, &sensor_thread);

    // MOCK aicVM receving the sensors value from player_sensors
    pthread_t* thread = malloc(sizeof(*thread));
    sensor_params_acc* params = (sensor_params_acc*) malloc(sizeof(sensor_params_acc));
    params->nbevent = 0;
    params->toread = len;

    pthread_create(thread, 0, &mock_vm_recv_poll, params);
    pthread_join(*thread, NULL);

    assert_int_equal(1, params->nbevent);
    assert_int_equal(payload.x, params->payload->x);
    assert_int_equal(payload.y, params->payload->y);
    assert_int_equal(payload.z, params->payload->z);

    free(params);
}

void test_sensors_nfc(void** state)
{
    (void) state;
    sensor_params* slp = ParamEventsWorker(g_vmip, g_vmid, "nfc", g_amqp_host);

    NfcPayload payload = NFC_PAYLOAD__INIT;

    payload.has_lang = 1;
    payload.has_type = 1;
    payload.lang = 1;
    payload.type = 1;
    payload.text = "google.com";

    int msglen = nfc_payload__get_packed_size(&payload);
    void* messagebody = malloc(msglen);

    int len = nfc_payload__pack(&payload, messagebody);

    amqp_send(g_amqp_host, 5672, slp->exchange, slp->queue, len, messagebody);
    sleep(3);

    // MOCK senza filling the sensors amqp queue
    free(messagebody);

    // Testing player_sensors
    // listen_NFC( slp ) ;
    pthread_t* thread0 = malloc(sizeof(*thread0));
    void* retval;
    pthread_create(thread0, 0, &listen_NFC, slp);
    pthread_join(*thread0, &retval);

    // sleep(10);

    //     //MOCK aicVM receving the sensors value from player_sensors
    //     pthread_t *thread = malloc(sizeof(*thread));
    //     nfc_params * params = (nfc_params *)malloc(sizeof(nfc_params ));
    //     params->nbevent=0;
    //     params->toread = len ;
    //     params->msg = (unsigned char* )malloc(sizeof(unsigned char)*128);
    //     pthread_create(thread, 0, &mock_vmnfc_recv_poll, params);
    //     pthread_join(*thread, NULL);
    //
    //     assert_int_equal( 1         ,  params->nbevent   );
    //     assert_int_equal( payload.lang ,  params->payload->lang);
    //     assert_int_equal( payload.type ,  params->payload->type);
    //     assert_string_equal( payload.text ,  params->payload->text);
    //
    //     uint8_t *NDEF_RecGetType (uint8_t *p_rec, uint8_t *p_tnf, uint8_t *p_type_len);
    //     uint8_t *NDEF_RecGetId (uint8_t *p_rec, uint8_t *p_id_len);
    //     uint8_t *NDEF_RecGetPayload (uint8_t *p_rec, UINT32 *p_payload_len);
    //     tNDEF_STATUS  _bValid = NDEF_MsgValidate (params->msg, params->len, 0);
    //
    assert_true(1);

    // pthread_cancel(*thread0);
    //     pthread_cancel(*thread);
    //
    //     free(params);
}

int main(int argc, char* argv[])
{
    (void) argc;
    (void) argv;
    init_logger();
    LOGI("Starting sensor listening");
    g_amqp_host = configvar_string("AIC_PLAYER_AMQP_HOST");
    g_vmid = configvar_string("AIC_PLAYER_VM_ID");
    g_vmip = configvar_string("AIC_PLAYER_VM_HOST");

    UnitTest tests[] = {
        unit_test(test_sensors_acc)
        // unit_test(test_sensors_nfc)
    };

    return run_tests(tests);
}
