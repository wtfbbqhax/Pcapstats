/* Copyright (c) 2010-2012, Victor J. Roemer. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * 3. The name of the author may not be used to endorse or promote
 * products derived from this software without specific prior written
 * permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <signal.h>

#include <string.h>
#include <errno.h>
#include <getopt.h>

#include <sys/time.h>
#include <sys/resource.h>

#include <packet.h>
#include <pcap.h>

#include "cdefs.h"
#include "readconf.h"
#include "mesg.h"
#include "daemon.h"
#include "print-data.h"

#include "defragment.h"
#include "stream-tcp.h"
#include "flow.h"
#include "host.h"

// cmdline configurations
Options cmdline, options = basicopts;

const char *config_file = SYSCONFDIR "/" PACKAGE_CONFIG_FILE;
const char *progname;

pcap_t *pcap = NULL;

/* Getopt stuff */
const char *shortopts = "r:i:Tc:Vdq";
static struct option longopts[] = {
    {"read", required_argument, NULL, 'r'},
    {"interface", required_argument, NULL, 'i'},
    {"config-test", no_argument, NULL, 'T'},
    {"config-file", required_argument, NULL, 'c'},
    {"help", no_argument, NULL, 254},
    {"version", no_argument, NULL, 'V'},
    {"daemon", no_argument, NULL, 'd'},
    {0, 0, 0, 0}
};

/*
 *
 */
static void
packet_callback(uint8_t * user UNUSED, const struct pcap_pkthdr *pkthdr,
                 const uint8_t * pkt)
{
    Packet *packet = packet_create();

    int error = packet_decode(packet, pkt, pkthdr->caplen);

    if (error)
        return;

    /* defragment the packet */
    if (packet_is_fragment(packet) && defragment(packet) != 0)
        return;

    if (packet_protocol(packet) == IPPROTO_TCP)
        track_tcp(packet);

//    track_packet_flow(packet);
//    track_packet_host(packet);

#ifdef DEBUG
    if (!options.quiet && packet_paysize(packet)) {
        printf("--- packet data ----\n");
        print_data(packet_payload(packet), packet_paysize(packet));
        printf("\n");
    }
#endif
}

/* Catch SIGHUP to reload the configuration file */
void sighup()
{
    info("Caught SIGHUP; reloading the configuration");

    /* Read configuration from config file */
    if (reload_config_file(config_file, &options) < 0)
        warn("Failed to reload the configuration; Continuing.");
    else
        info("Successfully reloaded the configuration.");
}

/* Catch SIGTERM and terminate the application */
void sigterm()
{
    info("Caught SIGTERM; exiting");
    pcap_breakloop(pcap);
}

/* Catch SIGINT and terminate the application */
void sigint()
{
    info("Caught SIGINT; exiting");
    pcap_breakloop(pcap);
}

/* Display version info */
static void show_version()
{
    printf("%s version %s\n", PACKAGE, PACKAGE_VERSION);
    if (pcap_lib_version())
        printf("%s\n\n", pcap_lib_version());
    printf(
    "Copyright (C) 2010-2012, Victor J. Roemer\n"
    "Released under The BSD 3-Clause License\n");

}

/* Display the most simplistic proper usage */
static void show_usage()
{
    fprintf(stderr, "Usage: %s -i INTERFACE or -r PCAP\n", progname);
}

/* Display more comprehensive usage */
static void show_help()
{
    show_usage();
    fprintf(stderr,
    "Statistical analysis of network traffic.\n\n"
    "\t-i, --interface=INTERFACE  live packet capture interface to read\n"
    "\t-r, --read=PCAP            static packet capture to read in\n"
    "\t-c, --config-file=FILE     specify alternate config file\n"
    "\t-T, --config-test          test the config file and exit\n"
    "\t-d, --daemon               run as a daemon\n\n"
    "\t--help                     display this help and exit\n"
    "\t-V, --version              output version information and exit\n\n");

    printf("Report bugs to <%s>.\n", PACKAGE_BUGREPORT);
}

/* Parse command line arguements */
static void parse_args(int argc, char *argv[], Options *options)
{
    int ch;
    int idx = 0;

    progname = argv[0];

    opterr = 0;
    while ((ch = getopt_long(argc, argv, shortopts, longopts, &idx)) != -1)
        switch (ch) {
            case 'q':
                options->quiet = true;
                break;
            case 'd':
                options->daemonize = true;
                break;
            case 'i':
                options->interface = optarg;
                break;
            case 'r':
                options->pcapfile = optarg;
                break;
            case 'c':
                config_file = optarg;
                break;
            case 'T':
                options->config_test = true;
                break;
            case 254:
                show_help();
                exit(1);
            case 'V':
                show_version();
                exit(1);
            default:
                fatal("Try `%s --help' for more information.", progname);
        }

    if (((!options->interface && !options->pcapfile) ||
        (options->interface && options->pcapfile)) &&
        !options->config_test) {
        show_usage();
        exit(1);
    }

    if ((options->daemonize && options->pcapfile) ||
        (options->daemonize && options->config_test)) {
        show_usage();
        exit(1);
    }
}

void dump_stats()
{
    const struct packet_stats *stats;

    packet_stats(&stats);

    mesg("Analyzed %u packets",
        stats->total_packets - stats->total_errors);
    mesg("Failed analysis on %u packets\n", stats->total_errors);

    if (stats->mplss_packets) {
        mesg("MPLS Packets      %u", stats->mplss_packets);
        mesg("MPLS Bytes        %u", stats->mplss_bytes);
        mesg("MPLS Too Short    %u", stats->mplss_tooshort);
    }

    if (stats->pppoes_packets) {
        mesg("PPPOE Packets     %u", stats->pppoes_packets);
        mesg("PPPOE Bytes       %u", stats->pppoes_bytes);
        mesg("PPPOE Too Short   %u", stats->pppoes_tooshort);
    }

    if (stats->ppps_packets) {
        mesg("PPP Packets       %u", stats->ppps_packets);
        mesg("PPP Bytes         %u", stats->ppps_bytes);
        mesg("PPP Too Short     %u", stats->ppps_tooshort);
    }

    if (stats->ipxs_packets) {
        mesg("IPX Packets       %u", stats->ipxs_packets);
        mesg("IPX Bytes         %u", stats->ipxs_bytes);
        mesg("IPX Bad Checksum  %u", stats->ipxs_badsum);
        mesg("IPX Too Short     %u", stats->ipxs_tooshort);
    }

    if (stats->ips_packets) {
        uint64_t ip_rejects = stats->ips_badsum +
          stats->ips_tooshort + stats->ips_badhlen +
          stats->ips_badlen;

        mesg("IP Packets        %u", stats->ips_packets);
        mesg("IP Bytes          %u", stats->ips_bytes);
        mesg("IP Fragments      %u", stats->ips_fragments);
        mesg("IP Rejects        %"PRIu64, ip_rejects);
        if (ip_rejects) {
            mesg("  IP Bad Checksum   %u", stats->ips_badsum);
            mesg("  IP Too Short      %u", stats->ips_tooshort);
            mesg("  IP Bad Hdr Length %u", stats->ips_badhlen);
            mesg("  IP Bad Length     %u", stats->ips_badlen);
        }
    }

    if (stats->tcps_packets) {
        uint64_t tcp_rejects = stats->tcps_badsum +
            stats->tcps_badoff + stats->tcps_tooshort;

        mesg("TCP Packets       %u", stats->tcps_packets);
        mesg("TCP Bytes         %u", stats->tcps_bytes);
        mesg("TCP Rejects       %"PRIu64, tcp_rejects);
        if (tcp_rejects) {
            mesg("  TCP Bad Checksum  %u", stats->tcps_badsum);
            mesg("  TCP Bad Offset    %u", stats->tcps_badoff);
            mesg("  TCP Too Short     %u", stats->tcps_tooshort);
        }
    }

    if (stats->udps_packets) {
        mesg("UDP Packets       %u", stats->udps_packets);
        mesg("UDP Bytes         %u", stats->udps_bytes);
        mesg("UDP Bad Checksum  %u", stats->udps_badsum);
        mesg("UDP Zero Checksum %u", stats->udps_nosum);
        mesg("UDP Too Short     %u", stats->udps_tooshort);
    }

    if (stats->sctps_packets) {
        mesg("SCTP Packets      %u", stats->sctps_packets);
        mesg("SCTP Bytes        %u", stats->sctps_bytes);
        mesg("SCTP Bad Checksum %u", stats->sctps_badsum);
        mesg("SCTP Bad Type     %u", stats->sctps_badtype);
        mesg("SCTP Too Short    %u", stats->sctps_tooshort);
    }

    if (stats->icmps_packets) {
        mesg("ICMP Packets      %u", stats->icmps_packets);
        mesg("ICMP Bytes        %u", stats->icmps_bytes);
        mesg("ICMP Bad Checksum %u", stats->icmps_badsum);
        mesg("ICMP Bad Type     %u", stats->icmps_badtype);
        mesg("ICMP Bad Code     %u", stats->icmps_badcode);
        mesg("ICMP Too Short    %u", stats->icmps_tooshort);
    }
}

int watch_signal(int sig, void (*callback)())
{
    struct sigaction sa;
    sa.sa_handler = callback;
    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, sig);
    sa.sa_flags = 0;
    if (sigaction(sig, &sa, NULL) < 0)
        return 1;
}

/*
 *
 */
int main(int argc, char *argv[])
{
    char errbuf[PCAP_ERRBUF_SIZE];

    parse_args(argc, argv, &options);

    /* Read configuration from config file */
    if (read_config_file(config_file, &options) < 0) {
        info("Config is invalid. Quiting");
        return 1;
    }

    if (options.config_test) {
        info("Config is valid. Quiting");
        return 0;
    }

    if (watch_signal(SIGTERM, sigterm))
        return 1;

    if (watch_signal(SIGHUP, sighup))
        return 1;

    /* if we want to daemonize */
    if (options.daemonize && daemonize() < 0) {
        fatal("Failed to daemonize: %s", strerror(errno));
    }

    /* Spinup backend components */
    frag_table_init();
    tcpssn_table_init( );
//    flow_table_init();
//    host_table_init();

    /* Start processing data */
    /* Setup live interface recording */
    if (options.interface) {
        if (watch_signal(SIGINT, sigint))
            return 1;

        pcap = pcap_open_live(options.interface, BUFSIZ, 1, 1000,
            errbuf);

        if (!pcap)
            fatal("%s (%s)", options.interface, errbuf);
    }
    /* Setup packet capture readback mode */
    if (options.pcapfile) {
        pcap = pcap_open_offline(options.pcapfile, errbuf);

        if (!pcap)
            fatal("%s (%s)", options.pcapfile, errbuf);
    }

    if (packet_set_datalink(pcap_datalink(pcap)) == -1)
        fatal("datalink type is not supported (%u)",
            pcap_datalink(pcap));

    if (pcap_loop(pcap, -1, packet_callback, NULL) == -1)
        fatal("%s", pcap_geterr(pcap));

    dump_stats();
    pcap_close(pcap);

    frag_table_finalize();
    tcpssn_table_finalize( );
//    flow_table_finalize();
//    host_table_finalize();

    return 0;
}

