/*
   Copyright (C) 2016-2017 Brian Stepp 
      steppnasty@gmail.com

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307, USA.

   The GNU General Public License is contained in the file COPYING.
*/

#define LOG_TAG "mm-daemon-sock"

#include "mm_daemon_sock.h"
#include "mm_daemon_util.h"

static mm_daemon_socket_t *mm_daemon_sock_create(mm_daemon_thread_info *info,
        int dev_id)
{
    struct sockaddr_un sockaddr;
    mm_daemon_socket_t *mm_sock = NULL;
    int sock_fd;

    mm_sock = (mm_daemon_socket_t *)calloc(1,
            sizeof(mm_daemon_socket_t));
    if (mm_sock == NULL)
        goto sock_alloc_fail;
    info->obj = (void *)mm_sock;

    sock_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (sock_fd < 0)
        goto socket_fail;
    memset(&sockaddr, 0, sizeof(sockaddr));
    sockaddr.sun_family = AF_UNIX;
    snprintf(sockaddr.sun_path, sizeof(sockaddr.sun_path),
            "/data/cam_socket%d", dev_id);
    if ((unlink(sockaddr.sun_path)) != 0 && errno != ENOENT)
        ALOGE("%s: failed to unlink socket '%s': %s", __FUNCTION__,
                sockaddr.sun_path, strerror(errno));
    else {
        if ((bind(sock_fd, (struct sockaddr *)&sockaddr,
                sizeof(sockaddr))) == 0) {
            ALOGI("%s: bound sock_fd=%d %s", __FUNCTION__, sock_fd,
                    sockaddr.sun_path);
            chmod(sockaddr.sun_path, 0660);
            mm_sock->dev_fd = sock_fd;
            mm_sock->dev_id = dev_id;
            mm_sock->sock_addr = sockaddr;
            return mm_sock;
        } else
            ALOGE("%s: unable to bind socket", __FUNCTION__);
    }

    close(sock_fd);
socket_fail:
    free(mm_sock);
sock_alloc_fail:
    ALOGE("%s: Error creating socket", __FUNCTION__);
    return NULL;
}

static int mm_daemon_proc_packet(mm_daemon_thread_info *info,
        cam_sock_packet_t *packet, int rcvd_fd)
{
    struct mm_daemon_sk_pkt *sk_pkt;
    mm_daemon_cfg_cmd_t cmd;

    switch (packet->msg_type) {
    case CAM_MAPPING_TYPE_FD_MAPPING:
        cmd = CFG_CMD_SK_PKT_MAP;
        break;
    case CAM_MAPPING_TYPE_FD_UNMAPPING:
        cmd = CFG_CMD_SK_PKT_UNMAP;
        break;
    default:
        return -EINVAL;
        break;
    }

    if (mm_daemon_util_set_thread_state(info, STATE_LOCKED) < 0)
        return -1;
   
    sk_pkt = (struct mm_daemon_sk_pkt *)calloc(1, sizeof(
            struct mm_daemon_sk_pkt));

    sk_pkt->cam_idx = info->sid;
    sk_pkt->fd = rcvd_fd;
    sk_pkt->data = (void *)packet;

    pthread_mutex_lock(&info->lock);
    mm_daemon_util_pipe_cmd(info->cb_pfd, cmd, (uint32_t)sk_pkt);
    pthread_cond_wait(&info->cond, &info->lock);
    pthread_mutex_unlock(&info->lock);
    free(sk_pkt);

    return 0;
}

static void *mm_daemon_sock_thread(void *data)
{
    mm_daemon_thread_info *info = (mm_daemon_thread_info *)data;
    mm_daemon_socket_t *mm_sock = NULL;
    cam_sock_packet_t packet;
    struct msghdr msgh;
    struct iovec iov[1];
    struct cmsghdr *cmsghp = NULL;
    char control[CMSG_SPACE(sizeof(int))];
    int rcvd_fd = -1;
    int rc = 0;
    int dev_id;

    sscanf(info->devpath, "/dev/video%u", &dev_id);
    mm_sock = mm_daemon_sock_create(info, dev_id);
    if (!mm_sock) {
        mm_daemon_util_pipe_cmd(info->cb_pfd, CFG_CMD_ERR, info->type);
        return NULL;
    }
    
    do {
        memset(&msgh, 0, sizeof(msgh));
        memset(&packet, 0, sizeof(cam_sock_packet_t));
        msgh.msg_name = NULL;
        msgh.msg_namelen = 0;
        msgh.msg_control = control;
        msgh.msg_controllen = sizeof(control);

        iov[0].iov_base = &packet;
        iov[0].iov_len = sizeof(cam_sock_packet_t);

        msgh.msg_iov = iov;
        msgh.msg_iovlen = 1;

        if (mm_daemon_util_set_thread_state(info, STATE_POLL) < 0)
            break;
        rc = recvmsg(mm_sock->dev_fd, &(msgh), 0);
        if (mm_daemon_util_set_thread_state(info, STATE_BUSY) < 0)
            break;
        if (((cmsghp = CMSG_FIRSTHDR(&msgh)) != NULL) &&
                (cmsghp->cmsg_len == CMSG_LEN(sizeof(int)))) {
            if (cmsghp->cmsg_level == SOL_SOCKET &&
                    cmsghp->cmsg_type == SCM_RIGHTS)
                rcvd_fd = *((int *)CMSG_DATA(cmsghp));
            else
                ALOGE("%s: Unexpected Control Msg", __FUNCTION__);
        }

        if ((mm_daemon_util_set_thread_state(info, 0) < 0) ||
                (mm_daemon_proc_packet(info, &packet, rcvd_fd) < 0))
            break;
    } while (mm_daemon_util_set_thread_state(info, 0) == 0);
    if (mm_sock) {
        unlink(mm_sock->sock_addr.sun_path);
        close(mm_sock->dev_fd);
        mm_sock->dev_fd = 0;
        free(mm_sock);
    }
    mm_daemon_util_set_thread_state(info, STATE_STOPPED);
    return NULL;
}

static void mm_daemon_sock_stop(mm_daemon_thread_info *info)
{
    int socket_fd;
    struct sockaddr_un sock_addr;
    struct msghdr msgh;
    struct iovec iov[1];
    mm_daemon_socket_t *mm_sock = (mm_daemon_socket_t *)info->obj;
    cam_sock_packet_t packet;

    socket_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (socket_fd < 0)
        return;

    memset(&packet, 0, sizeof(packet));
    memset(&sock_addr, 0, sizeof(sock_addr));
    memset(&msgh, 0, sizeof(msgh));
    sock_addr.sun_family = AF_UNIX;
    snprintf(sock_addr.sun_path, sizeof(sock_addr.sun_path),
            "/data/cam_socket%d", mm_sock->dev_id);
    if ((connect(socket_fd, (struct sockaddr *)&sock_addr,
            sizeof(sock_addr))) != 0)
        goto connect_fail;

    packet.msg_type = CAM_MAPPING_TYPE_MAX;
    msgh.msg_name = NULL;
    msgh.msg_namelen = 0;
    iov[0].iov_base = &packet;
    iov[0].iov_len = sizeof(packet);
    msgh.msg_iov = iov;
    msgh.msg_iovlen = 1;
    msgh.msg_control = NULL;
    msgh.msg_controllen = 0;

    sendmsg(socket_fd, &(msgh), 0);
connect_fail:
    close(socket_fd);
}

static struct mm_daemon_thread_ops mm_daemon_sock_ops = {
    .stop = mm_daemon_sock_stop,
    .thread = mm_daemon_sock_thread,
};

void mm_daemon_sock_load(mm_daemon_sd_info *sd)
{
    sd->ops = (void *)&mm_daemon_sock_ops;
}
