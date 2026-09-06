/*
 * Drives hostile and well-behaved RPC peers concurrently against a real gemd.
 * All traffic is confined to the private socket created by the test runner.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */
#define _POSIX_C_SOURCE 200809L
#include "_gem.h"

#include <assert.h>
#include <arpa/inet.h>
#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

static int input_fd;
static struct sockaddr_in input_peer;

static void input_listen(void)
{
    struct sockaddr_in address = {0};
    struct timeval timeout = {1, 0};
    input_fd = socket(AF_INET, SOCK_DGRAM, 0);
    assert(input_fd >= 0);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(5003);
    assert(bind(input_fd, (struct sockaddr *) &address, sizeof(address)) == 0);
    assert(setsockopt(input_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == 0);
}

static void input_send(WORD type, WORD first, WORD second)
{
    uint16_t packet[3] = {htons(type), htons(first), htons(second)};
    assert(sendto(input_fd, packet, sizeof(packet), 0,
        (struct sockaddr *) &input_peer, sizeof(input_peer)) == sizeof(packet));
}

static int connect_peer(void)
{
    struct sockaddr_un address = {0};
    struct timeval timeout = {1, 0};
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    assert(fd >= 0);
    address.sun_family = AF_UNIX;
    assert(strlen(gem_rpc_socket_path()) < sizeof(address.sun_path));
    strcpy(address.sun_path, gem_rpc_socket_path());
    assert(connect(fd, (struct sockaddr *) &address, sizeof(address)) == 0);
    assert(setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == 0);
    assert(setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) == 0);
    return fd;
}

static void send_request(int fd, uint16_t opcode, const void *data, uint32_t size)
{
    gem_rpc_header_t header = {GEM_RPC_MAGIC, GEM_RPC_VERSION, opcode, size};
    assert(send(fd, &header, sizeof(header), MSG_NOSIGNAL) == sizeof(header));
    if (size) assert(send(fd, data, size, MSG_NOSIGNAL) == size);
}

static int32_t receive_reply(int fd, void *out, size_t size)
{
    gem_rpc_reply_t reply;
    unsigned char discard[GEM_RPC_PAYLOAD_MAX];
    assert(recv(fd, &reply, sizeof(reply), MSG_WAITALL) == sizeof(reply));
    assert(reply.magic == GEM_RPC_MAGIC && reply.size <= sizeof(discard));
    assert(!out || reply.size == size);
    if (reply.size)
        assert(recv(fd, out ? out : discard, reply.size, MSG_WAITALL) == reply.size);
    return reply.status;
}

static int32_t call(int fd, uint16_t opcode, const void *data, uint32_t size)
{
    send_request(fd, opcode, data, size);
    return receive_reply(fd, NULL, 0);
}

static WORD start(int fd)
{
    WORD id = (WORD) call(fd, GEM_RPC_APPL_INIT, NULL, 0);
    assert(id > 0);
    assert(call(fd, GEM_RPC_APPL_INIT, NULL, 0) == id);
    assert(call(fd, GEM_RPC_GRAF_HANDLE, NULL, 0) > 0);
    return id;
}

static WORD window(int fd, WORD x)
{
    gem_rpc_wind_create_req_t create = {NAME | CLOSER | MOVER, x, 60, 140, 140};
    WORD handle = (WORD) call(fd, GEM_RPC_WIND_CREATE, &create, sizeof(create));
    gem_rpc_wind_open_req_t open = {handle, x, 60, 140, 140};
    assert(handle > 0 && call(fd, GEM_RPC_WIND_OPEN, &open, sizeof(open)));
    {
        gem_rpc_wind_get_req_t get = {handle, WF_KIND};
        gem_rpc_wind_get_rsp_t reply;
        send_request(fd, GEM_RPC_WIND_GET, &get, sizeof(get));
        assert(receive_reply(fd, &reply, sizeof(reply)) == 1);
        assert(reply.w1 == create.kind && !reply.w2 && !reply.w3 && !reply.w4);
    }
    return handle;
}

static void stalls(int healthy)
{
    int slow = connect_peer();
    gem_rpc_header_t header = {GEM_RPC_MAGIC, GEM_RPC_VERSION, GEM_RPC_APPL_INIT, 0};
    unsigned char one = (unsigned char) GEM_RPC_MAGIC;
    assert(send(slow, &one, 1, MSG_NOSIGNAL) == 1);
    assert(call(healthy, GEM_RPC_GRAF_HANDLE, NULL, 0) > 0);
    close(slow);
    slow = connect_peer();
    header.opcode = GEM_RPC_V_PLINE;
    header.size = sizeof(gem_rpc_pline_req_t);
    assert(send(slow, &header, sizeof(header), MSG_NOSIGNAL) == sizeof(header));
    assert(call(healthy, GEM_RPC_GRAF_HANDLE, NULL, 0) > 0);
    close(slow);
    slow = connect_peer();
    (void) start(slow);
    header.opcode = GEM_RPC_GRAF_HANDLE; header.size = 0;
    for (int i = 0; i < 10000; ++i) {
        ssize_t written = send(slow, &header, sizeof(header), MSG_NOSIGNAL | MSG_DONTWAIT);
        if (written != sizeof(header)) break;
    }
    assert(call(healthy, GEM_RPC_GRAF_HANDLE, NULL, 0) > 0);
    close(slow);
    puts("partial header/body cannot stall another application");
}

static gem_rpc_menu_bar_req_t menu(void)
{
    gem_rpc_menu_bar_req_t req = {0};
    WORD i;
    req.show = 1; req.object_count = 7; req.string_count = 2;
    for (i = 0; i < 7; ++i) {
        req.objects[i].ob_head = req.objects[i].ob_tail = NIL;
        req.objects[i].ob_type = G_IBOX;
        req.objects[i].ob_width = 100; req.objects[i].ob_height = 20;
    }
    req.objects[0].ob_next = NIL; req.objects[0].ob_head = 1; req.objects[0].ob_tail = 4;
    req.objects[1].ob_next = 4; req.objects[1].ob_head = req.objects[1].ob_tail = 2;
    req.objects[1].ob_type = G_BOX;
    req.objects[2].ob_next = 1; req.objects[2].ob_head = req.objects[2].ob_tail = 3;
    req.objects[3].ob_next = 2; req.objects[3].ob_type = G_TITLE;
    req.objects[4].ob_next = 0; req.objects[4].ob_head = req.objects[4].ob_tail = 5;
    req.objects[5].ob_next = 4; req.objects[5].ob_head = req.objects[5].ob_tail = 6;
    req.objects[5].ob_type = G_BOX;
    req.objects[6].ob_next = 5; req.objects[6].ob_type = G_STRING;
    req.objects[6].ob_flags = LASTOB;
    req.strings[0].object = 3; strcpy(req.strings[0].text, "File");
    req.strings[1].object = 6; strcpy(req.strings[1].text, "Exit");
    return req;
}

static void bad_menus(int fd)
{
    gem_rpc_menu_bar_req_t good = menu(), bad;
    gem_rpc_menu_tnormal_req_t normal = {32767, 1};
    assert(call(fd, GEM_RPC_MENU_BAR, &good, sizeof(good)) == 1);
    assert(call(fd, GEM_RPC_MENU_TNORMAL, &normal, sizeof(normal)) == 0);
    bad = good; bad.objects[1].ob_head = 1;
    assert(call(fd, GEM_RPC_MENU_BAR, &bad, sizeof(bad)) == -1);
    bad = good; bad.objects[3].ob_next = 3;
    assert(call(fd, GEM_RPC_MENU_BAR, &bad, sizeof(bad)) == -1);
    bad = good; bad.objects[6].ob_flags = INDIRECT | LASTOB;
    assert(call(fd, GEM_RPC_MENU_BAR, &bad, sizeof(bad)) == -1);
    bad = good; bad.objects[6].ob_flags = 0;
    assert(call(fd, GEM_RPC_MENU_BAR, &bad, sizeof(bad)) == -1);
    bad = good; bad.strings[1].object = 5;
    assert(call(fd, GEM_RPC_MENU_BAR, &bad, sizeof(bad)) == -1);
    /* Systematically mutate each tree link, including valid-shaped variants.
     * All rejected graphs must leave the next request and menu usable. */
    for (int object = 0; object < 7; ++object) {
        for (int field = 0; field < 3; ++field) {
            for (WORD value = -2; value <= 8; ++value) {
                int valid;
                int32_t status;
                bad = good;
                if (field == 0) bad.objects[object].ob_next = value;
                if (field == 1) bad.objects[object].ob_head = value;
                if (field == 2) bad.objects[object].ob_tail = value;
                valid = gem_rpc_valid_request(GEM_RPC_MENU_BAR, &bad, sizeof(bad));
                status = call(fd, GEM_RPC_MENU_BAR, &bad, sizeof(bad));
                assert(valid ? status >= 0 : status == -1);
            }
        }
    }
    assert(call(fd, GEM_RPC_MENU_BAR, &good, sizeof(good)) == 1);
    normal.title = 3;
    assert(call(fd, GEM_RPC_MENU_TNORMAL, &normal, sizeof(normal)) == 1);
    puts("invalid menu indices, cycles, pointer forms and terminators rejected");
}

static void ownership(int a, int b, WORD owned)
{
    gem_rpc_handle_req_t handle = {owned};
    gem_rpc_wind_set_req_t set = {owned, WF_NAME, 1, 0, 0, 0};
    gem_rpc_wind_set_str_req_t text = {0};
    assert(call(b, GEM_RPC_WIND_CLOSE, &handle, sizeof(handle)) == 0);
    assert(call(b, GEM_RPC_WIND_DELETE, &handle, sizeof(handle)) == 0);
    set.field = WF_TOP;
    assert(call(b, GEM_RPC_WIND_SET, &set, sizeof(set)) == 0);
    text.handle = owned; text.field = WF_NAME; strcpy(text.text, "intruder");
    assert(call(b, GEM_RPC_WIND_SET_STR, &text, sizeof(text)) == 0);
    set.field = WF_NAME;
    assert(call(a, GEM_RPC_WIND_SET, &set, sizeof(set)) == -1);
    text.handle = owned; strcpy(text.text, "owner");
    assert(call(a, GEM_RPC_WIND_SET_STR, &text, sizeof(text)) == 1);
    puts("foreign window mutations and serialized process pointers rejected");
}

static int bit(int x, int y)
{
    FILE *file = fopen(getenv("GEM_RASTA_FRAMEBUFFER"), "rb");
    int byte;
    assert(file && fseek(file, y * 113 + x / 8, SEEK_SET) == 0);
    byte = fgetc(file); fclose(file);
    assert(byte != EOF);
    return (byte & (0x80 >> (x % 8))) != 0;
}

static void drawing(int a, int b, int c)
{
    gem_rpc_opnvwk_req_t open = {{0}};
    gem_rpc_color_req_t ink = {1, WHITE}, paper = {1, BLACK};
    gem_rpc_rect_req_t block = {1, {60, 110, 70, 120}};
    gem_rpc_clip_req_t clip = {1, 1, {240, 110, 250, 120}};
    gem_rpc_handle_req_t handle = {1};
    assert(call(a, GEM_RPC_V_OPNVWK, &open, sizeof(open)) > 0);
    assert(call(b, GEM_RPC_V_OPNVWK, &open, sizeof(open)) > 0);
    assert(call(c, GEM_RPC_V_OPNVWK, &open, sizeof(open)) > 0);
    assert(call(a, GEM_RPC_VSF_COLOR, &ink, sizeof(ink)));
    assert(call(b, GEM_RPC_VSF_COLOR, &paper, sizeof(paper)));
    assert(call(b, GEM_RPC_VS_CLIP, &clip, sizeof(clip)));
    assert(call(a, GEM_RPC_V_BAR, &block, sizeof(block)));
    assert(bit(64, 114));
    /* Even an unclipped full-screen clear cannot erase another app. */
    assert(call(c, GEM_RPC_V_CLRWK, &handle, sizeof(handle)));
    assert(bit(64, 114));
    block.xy[0] = 240; block.xy[2] = 250;
    assert(call(b, GEM_RPC_V_BAR, &block, sizeof(block)));
    assert(!bit(244, 114));
    assert(call(b, GEM_RPC_VSF_COLOR, &ink, sizeof(ink)));
    assert(call(b, GEM_RPC_V_BAR, &block, sizeof(block)));
    assert(bit(244, 114));
    {
        gem_rpc_handle_word_req_t mode = {1, MD_XOR}, fill = {1, FIS_HOLLOW};
        assert(call(b, GEM_RPC_VSWR_MODE, &mode, sizeof(mode)));
        assert(call(b, GEM_RPC_VSF_INTERIOR, &fill, sizeof(fill)) == FIS_HOLLOW);
        assert(call(b, GEM_RPC_V_CLRWK, &handle, sizeof(handle)));
        assert(!bit(244, 114) && bit(64, 114));
        assert(call(b, GEM_RPC_V_BAR, &block, sizeof(block)));
        assert(!bit(244, 114));
    }
    puts("three clients retain independent drawing attributes and work areas");
}

static void timers(int a, int b)
{
    gem_rpc_evnt_multi_req_t timer = {0};
    timer.flags = MU_TIMER; timer.tlc = timer.thc = 65535;
    send_request(a, GEM_RPC_EVNT_MULTI, &timer, sizeof(timer));
    assert(call(b, GEM_RPC_GRAF_HANDLE, NULL, 0) > 0);
    (void) receive_reply(a, NULL, 0);
    puts("hostile multi-day timer is bounded without blocking another app");
}

static void input_routing(const int clients[3], const WORD windows[3])
{
    char subscription[4096];
    socklen_t size = sizeof(input_peer);
    gem_rpc_evnt_multi_req_t request = {0};
    gem_rpc_evnt_multi_rsp_t reply;
    assert(recvfrom(input_fd, subscription, sizeof(subscription), 0,
        (struct sockaddr *) &input_peer, &size) > 0);
    request.flags = MU_KEYBD | MU_TIMER;
    request.tlc = 2;
    for (int owner = 0; owner < 3; ++owner) {
        gem_rpc_wind_set_req_t top = {windows[owner], WF_TOP, 0, 0, 0, 0};
        assert(call(clients[owner], GEM_RPC_WIND_SET, &top, sizeof(top)));
        input_send(1, (WORD) (4 + owner), 0);
        input_send(2, (WORD) (4 + owner), 0);
        for (int i = 1; i <= 3; ++i) {
            int client = (owner + i) % 3;
            send_request(clients[client], GEM_RPC_EVNT_MULTI, &request, sizeof(request));
            (void) receive_reply(clients[client], &reply, sizeof(reply));
            if (client == owner)
                assert((reply.event & MU_KEYBD) && (reply.kr & 255) == 'a' + owner);
            else assert(!(reply.event & MU_KEYBD));
        }
    }
    puts("keyboard follows focus between three apps; background apps cannot consume it");
}

static void locks(int healthy)
{
    int owner = connect_peer();
    gem_rpc_wind_update_req_t begin = {BEG_UPDATE}, end = {END_UPDATE};
    struct pollfd ready = {healthy, POLLIN, 0};
    (void) start(owner);
    assert(call(owner, GEM_RPC_WIND_UPDATE, &begin, sizeof(begin)) == 1);
    send_request(healthy, GEM_RPC_WIND_UPDATE, &end, sizeof(end));
    assert(call(owner, GEM_RPC_WIND_UPDATE, &end, sizeof(end)) == 1);
    assert(receive_reply(healthy, NULL, 0) == 0);
    assert(call(owner, GEM_RPC_WIND_UPDATE, &begin, sizeof(begin)) == 1);
    close(owner);
    assert(call(healthy, GEM_RPC_GRAF_HANDLE, NULL, 0) > 0);
    owner = connect_peer(); (void) start(owner);
    assert(call(owner, GEM_RPC_WIND_UPDATE, &begin, sizeof(begin)) == 1);
    send_request(healthy, GEM_RPC_GRAF_HANDLE, NULL, 0);
    assert(poll(&ready, 1, 6500) == 1 && (ready.revents & POLLIN));
    assert(receive_reply(healthy, NULL, 0) > 0);
    close(owner);
    puts("unbalanced, disconnected and abandoned update locks cannot strand peers");
}

static void read_frame(unsigned char pixels[900 * 113])
{
    FILE *file = fopen(getenv("GEM_RASTA_FRAMEBUFFER"), "rb");
    assert(file && fread(pixels, 1, 900 * 113, file) == 900 * 113);
    assert(fclose(file) == 0);
}

static int frame_pixel(const unsigned char *pixels, int x, int y)
{
    return !!(pixels[y * 113 + x / 8] & (0x80 >> (x % 8)));
}

static void alert_frame(const unsigned char *before)
{
    unsigned char after[900 * 113];
    int left = 904, top = 900, right = -1, bottom = -1;
    read_frame(after);
    for (int y = 0; y < 900; ++y) {
        for (int x = 0; x < 904; ++x) {
            if (frame_pixel(before, x, y) == frame_pixel(after, x, y)) continue;
            if (x < left) left = x;
            if (x > right) right = x;
            if (y < top) top = y;
            if (y > bottom) bottom = y;
        }
    }
    assert(right - left > 40 && bottom - top > 40);
    for (int inset = 0; inset < 4; ++inset) {
        int ink = inset != 1;
        assert(frame_pixel(after, left + 20, top + inset) == ink);
        assert(frame_pixel(after, left + 20, bottom - inset) == ink);
        assert(frame_pixel(after, left + inset, top + 20) == ink);
        assert(frame_pixel(after, right - inset, top + 20) == ink);
    }
}

static void modal_waits(int healthy)
{
    int owner = connect_peer();
    gem_rpc_form_alert_req_t alert = {1, "[1][Security regression][OK]"};
    gem_rpc_fsel_t selector = {{0}, {0}, 0};
    struct timespec pause = {0, 50000000};
    unsigned char before[900 * 113];
    (void) start(owner);
    read_frame(before);
    send_request(owner, GEM_RPC_FORM_ALERT, &alert, sizeof(alert));
    nanosleep(&pause, NULL);
    assert(call(healthy, GEM_RPC_GRAF_HANDLE, NULL, 0) > 0);
    alert_frame(before);
    input_send(1, 40, 0);
    input_send(2, 40, 0);
    assert(receive_reply(owner, NULL, 0) == 1);
    send_request(owner, GEM_RPC_FORM_ALERT, &alert, sizeof(alert));
    nanosleep(&pause, NULL);
    close(owner);
    assert(call(healthy, GEM_RPC_GRAF_HANDLE, NULL, 0) > 0);
    owner = connect_peer(); (void) start(owner);
    strcpy(selector.path, "/tmp/*");
    send_request(owner, GEM_RPC_FSEL_INPUT, &selector, sizeof(selector));
    nanosleep(&pause, NULL);
    assert(call(healthy, GEM_RPC_GRAF_HANDLE, NULL, 0) > 0);
    close(owner);
    assert(call(healthy, GEM_RPC_GRAF_HANDLE, NULL, 0) > 0);
    puts("alert/selector waits service other apps and cancel on owner disconnect");
}

static uint32_t menu_hash(void)
{
    uint32_t hash = 2166136261u;
    FILE *file = fopen(getenv("GEM_RASTA_FRAMEBUFFER"), "rb");
    int i, byte;
    assert(file);
    for (i = 0; i < 22 * 113; ++i) {
        byte = fgetc(file); assert(byte != EOF);
        hash = (hash ^ (unsigned char) byte) * 16777619u;
    }
    fclose(file);
    return hash;
}

static void menu_lifetimes(int a, int b, int c)
{
    gem_rpc_menu_bar_req_t req = menu(), hide = {0};
    uint32_t active;
    assert(call(a, GEM_RPC_MENU_BAR, &req, sizeof(req)) == 1);
    strcpy(req.strings[0].text, "Second");
    assert(call(b, GEM_RPC_MENU_BAR, &req, sizeof(req)) == 1);
    strcpy(req.strings[0].text, "Third");
    assert(call(c, GEM_RPC_MENU_BAR, &req, sizeof(req)) == 1);
    active = menu_hash();
    assert(call(a, GEM_RPC_MENU_BAR, &hide, sizeof(hide)) == 1);
    assert(active == menu_hash());
    close(a);
    assert(call(b, GEM_RPC_GRAF_HANDLE, NULL, 0) > 0);
    close(c);
    assert(call(b, GEM_RPC_GRAF_HANDLE, NULL, 0) > 0);
    assert(active != menu_hash());
    puts("inactive menu removal and desktop/active-owner exit preserve the survivor");
}

int main(void)
{
    struct stat mode;
    int a = connect_peer(), b = connect_peer(), c = connect_peer();
    WORD a_id, b_id, c_id, a_window, b_window, c_window;
    setbuf(stdout, NULL);
    input_listen();
    assert(stat(gem_rpc_socket_path(), &mode) == 0 && (mode.st_mode & 0777) == 0600);
    assert(call(b, GEM_RPC_GRAF_HANDLE, NULL, 0) == 0);
    a_id = start(a); b_id = start(b); c_id = start(c);
    assert(a_id != b_id && a_id != c_id && b_id != c_id);
    stalls(a);
    a_window = window(a, 40); b_window = window(b, 220); c_window = window(c, 400);
    bad_menus(a);
    ownership(a, b, a_window);
    drawing(a, b, c);
    input_routing((int[3]) {a, b, c}, (WORD[3]) {a_window, b_window, c_window});
    timers(a, b);
    locks(b);
    modal_waits(b);
    menu_lifetimes(a, b, c);
    close(b);
    close(input_fd);
    puts("GEM security/multi-application regressions passed");
    return 0;
}
