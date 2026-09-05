/*
 * Exercises real libgem/gemd round trips, fragmented frames, bad lengths,
 * menu relocation, output arrays and persistent state across connections.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "_gem.h"

#include <assert.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

static int input_fd;
static struct sockaddr_in input_peer;

/* Receive the real Rasta subscription and drive that server's HID socket. */
static void input_listen(void)
{
    struct sockaddr_in address = {0};
    struct timeval timeout = {2, 0};
    input_fd = socket(AF_INET, SOCK_DGRAM, 0);
    assert(input_fd >= 0);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(5003);
    assert(bind(input_fd, (struct sockaddr *) &address, sizeof(address)) == 0);
    assert(setsockopt(input_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout,
        sizeof(timeout)) == 0);
}

static void input_send(WORD type, WORD first, WORD second)
{
    uint16_t packet[3] = {htons(type), htons(first), htons(second)};
    assert(sendto(input_fd, packet, sizeof(packet), 0,
        (struct sockaddr *) &input_peer, sizeof(input_peer)) == sizeof(packet));
}

static void keyboard_roundtrip(void)
{
    char subscription[4096];
    socklen_t size = sizeof(input_peer);
    WORD mx, my, mb, ks, kr, br, event;
    int found = 0;
    int attempt;

    assert(recvfrom(input_fd, subscription, sizeof(subscription), 0,
        (struct sockaddr *) &input_peer, &size) > 0);
    {
        int stranger = socket(AF_INET, SOCK_DGRAM, 0);
        uint16_t packet[4] = {htons(3), htons(777), htons(777), 0};
        assert(stranger >= 0);
        assert(sendto(stranger, packet, 6, 0, (struct sockaddr *) &input_peer,
            sizeof(input_peer)) == 6);
        close(stranger);
        assert(sendto(input_fd, packet, sizeof(packet), 0,
            (struct sockaddr *) &input_peer, sizeof(input_peer)) == sizeof(packet));
        graf_mkstate(&mx, &my, &mb, &ks);
        assert(mx != 777 && my != 777);
    }
    input_send(3, 60, 90);
    input_send(10, 60, 90);
    /* A pending WM_REDRAW must preserve the held mouse state, not return
     * the zero-filled response buffer and invent a release in the client. */
    graf_mkstate(&mx, &my, &mb, &ks);
    assert(mx == 60 && my == 90 && (mb & 1));
    event = evnt_multi(MU_MESAG | MU_TIMER, 1, 1, 1,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, (WORD[8]) {0}, 20, 0,
        &mx, &my, &mb, &ks, &kr, &br);
    assert(event == MU_MESAG);
    assert(mx == 60 && my == 90 && (mb & 1));
    input_send(11, 60, 90);
    input_send(1, 224, 0); /* Ctrl down, A down/up, Ctrl up in one burst. */
    input_send(1, 4, 0);
    input_send(2, 4, 0);
    input_send(2, 224, 0);
    input_send(1, 5, 0);
    input_send(2, 5, 0);
    for (attempt = 0; attempt < 20 && found < 2; ++attempt) {
        event = evnt_multi(MU_KEYBD | MU_TIMER, 1, 1, 1,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, 20, 0,
            &mx, &my, &mb, &ks, &kr, &br);
        if (event & MU_KEYBD) {
            if ((kr & 0xff) == 'a') {
                assert((ks & 4) != 0);
                ++found;
            } else if ((kr & 0xff) == 'b') {
                assert((ks & 4) == 0);
                ++found;
            }
        }
    }
    assert(found == 2);
    close(input_fd);
}

static void malformed_and_fragmented(void)
{
    struct sockaddr_un address = {0};
    gem_rpc_header_t header = {GEM_RPC_MAGIC, GEM_RPC_VERSION,
        GEM_RPC_WIND_GET, 0};
    gem_rpc_reply_t reply;
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    size_t i;

    assert(!gem_rpc_call(GEM_RPC_APPL_INIT, NULL, 1, NULL, NULL, 0));
    assert(!gem_rpc_call(GEM_RPC_APPL_INIT, NULL, 0, NULL, NULL, 1));
    assert(!gem_rpc_call(GEM_RPC_APPL_INIT, &header, GEM_RPC_PAYLOAD_MAX + 1,
        NULL, NULL, 0));
    assert(fd >= 0);
    address.sun_family = AF_UNIX;
    strcpy(address.sun_path, gem_rpc_socket_path());
    assert(connect(fd, (struct sockaddr *) &address, sizeof(address)) == 0);
    /* A short WIND_GET used to dispatch from uninitialized stack bytes. */
    for (i = 0; i < sizeof(header); ++i) {
        assert(send(fd, (char *) &header + i, 1, 0) == 1);
    }
    assert(recv(fd, &reply, sizeof(reply), MSG_WAITALL) == sizeof(reply));
    assert(reply.magic == GEM_RPC_MAGIC && reply.status == -1 && !reply.size);
    /* The next valid request on the same stream must stay aligned. */
    header.opcode = GEM_RPC_APPL_INIT;
    assert(send(fd, &header, sizeof(header), 0) == sizeof(header));
    assert(recv(fd, &reply, sizeof(reply), MSG_WAITALL) == sizeof(reply));
    assert(reply.status > 0);
    close(fd);
}

static void bad_counts(void)
{
    gem_rpc_pline_req_t points = {0};
    gem_rpc_menu_bar_req_t menu = {0};
    int32_t status = 0;

    points.count = 129;
    assert(gem_rpc_call(GEM_RPC_V_PLINE, &points, sizeof(points),
        &status, NULL, 0) && status == -1);
    menu.show = 1;
    menu.object_count = 1;
    menu.string_count = GEM_RPC_MENU_MAX_STRINGS + 1;
    assert(gem_rpc_call(GEM_RPC_MENU_BAR, &menu, sizeof(menu),
        &status, NULL, 0) && status == -1);
    menu.string_count = 0;
    menu.objects[0].ob_type = G_USERDEF;
    menu.objects[0].ob_spec = 0x1234;
    assert(gem_rpc_call(GEM_RPC_MENU_BAR, &menu, sizeof(menu),
        &status, NULL, 0) && status == -1);
}

static void menu_client(void)
{
    OBJECT tree[7] = {0};
    WORD i;
    char title[] = "File";
    char item[] = "Exit";

    for (i = 0; i < 7; ++i) {
        tree[i].ob_head = tree[i].ob_tail = NIL;
        tree[i].ob_type = G_IBOX;
        tree[i].ob_width = 100;
        tree[i].ob_height = 20;
    }
    tree[0].ob_next = NIL; tree[0].ob_head = 1; tree[0].ob_tail = 4;
    tree[1].ob_next = 4; tree[1].ob_head = tree[1].ob_tail = 2;
    tree[1].ob_type = G_BOX;
    tree[2].ob_next = 1; tree[2].ob_head = tree[2].ob_tail = 3;
    tree[3].ob_next = 2; tree[3].ob_type = G_TITLE;
    tree[3].ob_spec = (LONG) (intptr_t) title;
    tree[4].ob_next = 0; tree[4].ob_head = tree[4].ob_tail = 5;
    tree[5].ob_next = 4; tree[5].ob_head = tree[5].ob_tail = 6;
    tree[5].ob_type = G_BOX;
    tree[6].ob_next = 5; tree[6].ob_type = G_STRING;
    tree[6].ob_flags = LASTOB;
    tree[6].ob_spec = (LONG) (intptr_t) item;
    assert(appl_init() > 0);
    assert(menu_bar(tree, 1));
    assert(menu_bar(tree, 1));
    assert(menu_tnormal(tree, 3, 1));
    /* Deliberately disconnect without menu_bar(false) or appl_exit(). */
}

int main(int argc, char **argv)
{
    WORD cw, ch, bw, bh, handle, window;
    WORD work_in[11] = {0}, work_out[57] = {0};
    WORD min_ade, max_ade, distances[5], width, effects[3];
    WORD x, y, w, h, mx, my, buttons, keys;
    WORD extent[8], clip[4] = {30, 30, 60, 60};
    char scrap[1024] = {0};
    char path[] = "/tmp/GEM proxy path with spaces/";

    if (argc > 1) {
        menu_client();
        return 0;
    }

    input_listen();

    assert(appl_init() > 0);
    handle = graf_handle(&cw, &ch, &bw, &bh);
    assert(handle > 0 && cw > 0 && ch > 0);
    v_opnvwk(work_in, &handle, work_out);
    assert(handle > 0 && work_out[0] == 903 && work_out[1] == 899);
    assert(vqt_fontinfo(handle, &min_ade, &max_ade, distances, &width, effects));
    assert(min_ade <= 'A' && max_ade >= 'z' && width > 0 && distances[3] > 0);
    assert(vqt_fontinfo(handle, NULL, NULL, NULL, NULL, NULL));
    graf_mkstate(&mx, &my, &buttons, &keys);
    assert(mx >= 0 && my >= 0);
    assert(scrp_write(path) && scrp_read(scrap) && !strcmp(path, scrap));
    window = wind_create(NAME | CLOSER | MOVER, 30, 40, 200, 150);
    assert(window > 0 && wind_set_str(window, WF_NAME, "RPC title"));
    assert(wind_open(window, 30, 40, 200, 150));
    assert(wind_get(window, WF_WXYWH, &x, &y, &w, &h));
    assert(x == 30 && y == 40 && w == 200 && h == 150);
    assert(wind_set(window, WF_WXYWH, 40, 50, 210, 160));
    assert(wind_get(window, WF_WXYWH, &x, &y, &w, &h));
    assert(x == 40 && y == 50 && w == 210 && h == 160);
    assert(wind_find(50, 70) == window);
    assert(wind_calc(WC_WORK, NAME | CLOSER | MOVER,
        40, 50, 210, 160, &x, &y, &w, &h));
    assert(w > 0 && h > 0 && w <= 210 && h < 160);
    keyboard_roundtrip();
    assert(wind_update(BEG_UPDATE));
    vs_clip(handle, 1, clip);
    vsf_color(handle, BLACK);
    v_bar(handle, clip);
    v_gtext(handle, 30, 50, (const BYTE *) "RPC");
    assert(vqt_extent(handle, "RPC", extent));
    assert(extent[2] > 0);
    vs_clip(handle, 0, NULL);
    assert(wind_update(END_UPDATE));
    bad_counts();
    malformed_and_fragmented();
    {
        pid_t child = fork();
        int status;
        assert(child >= 0);
        if (child == 0) {
            execl(argv[0], argv[0], "menu-client", (char *) NULL);
            _exit(127);
        }
        assert(waitpid(child, &status, 0) == child && WIFEXITED(status) &&
            WEXITSTATUS(status) == 0);
        assert(wind_set(window, WF_WXYWH, 50, 60, 200, 150));
    }
    assert(wind_close(window) && wind_delete(window));
    v_clsvwk(handle);
    assert(appl_exit());
    assert(appl_init() > 0);
    memset(scrap, 0, sizeof(scrap));
    assert(scrp_read(scrap) && !strcmp(scrap, path));
    assert(appl_exit());
    puts("GEM RPC round trips, malformed frames and reconnection passed.");
    return 0;
}
