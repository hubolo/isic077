#include "isic.h"

/* This is tuned for ethernet sized frames (1500 bytes)
 * For user over a modem or frame (or other) you will have to change the
 * 'rand() & 0x4ff' line below.  The 0x4ff needs to be less than the size of
 * the frame size minus the length of the ip header (20 bytes IIRC) minus the
 * length of the TCP header.
 */


/* Variables shared between main and the signal handler so we can display
 * output if ctrl-c'd
 */
u_int seed = 0;
u_long acx = 0;
struct timeval starttime;
u_long datapushed = 0;			/* How many bytes we pushed */


int
main(int argc, char **argv)
{
	int c;
	u_char *buf = NULL;
	u_short	*payload = NULL;
	u_int payload_s = 0;
	int packet_len = 0;

	struct ip *ip_hdr = NULL;
	struct tcphdr *tcp = NULL;
	u_short *ip_opts = NULL;

	/* libnet variables */
	char errbuf[LIBNET_ERRBUF_SIZE];
	libnet_t *l;
	char *device = NULL;

	/* Packet Variables */
	u_int32_t src_ip = 0, dst_ip = 0;
	u_short src_prt = 0, dst_prt = 0;
	u_int32_t src_ip_b = 0, src_ip_e = 0,dst_ip_b = 0, dst_ip_e = 0;
	u_int32_t src_len = 0,dst_len = 0;
	u_char tos, ttl, ver;
	u_int id, frag_off;
	u_int ipopt_len;


	/* Functionality Variables */
	int src_ip_rand = 0, dst_ip_rand = 0;
	struct timeval tv, tv2;
	float sec;
	unsigned int cx = 0;
	u_long max_pushed = 10240;		/* 10MB/sec */
	u_long num_to_send = 0xffffffffffffffff;	/* Send 4billion packets */
	u_long skip = 0; 			/* Skip how many packets */
	int printout = 0;			/* Debugging */
	int dst_port_rand = 0;
	int src_port_rand = 0;
	char *tmp_port = NULL;
	u_int repeat = 1;

	/* Defaults */
	float FragPct	=	10;
	float BadIPVer	=	10;
	float IPOpts	=	10;
	float UrgPct	=	10;
	float TCPOptsPct=	10;
	float TCPCksm	=	10;



	/* Not crypto strong randomness but we don't really care.  And this  *
	 * gives us a way to determine the seed while the program is running *
 	 * if we need to repeat the results				     */
	seed = getpid();

	/* Initialize libnet context, Root priviledges are required.*/ 
	l = libnet_init(
            LIBNET_RAW4_ADV,                        /* injection type */
            device,                                 /* network interface */
            errbuf);                                /* error buffer */

	if (l == NULL) {
	  fprintf(stderr, "libnet_init() failed: %s", errbuf);
	  exit( -1 );
	}
	

	while((c = getopt(argc, argv, "a:b:y:z:hd:s:r:m:k:Dp:V:F:I:u:T:t:vx:l:")) != EOF){
	  switch (c) {
	   case 'h':
		usage(argv[0]);
		exit(0);
		break;
	   case 'd':
		dst_port_rand = 1;
		if ( (tmp_port = index(optarg, ',')) != NULL ) {
			*tmp_port++ = '\0';
			dst_port_rand = 0;
			dst_prt = htons((u_int) atol(tmp_port));
		}
		if ( strcmp(optarg, "rand") == 0 ) {
			printf("Using random dest IP's\n");
			dst_ip = 1;	/* Just to pass sanity checks */
			dst_ip_rand = 1;
			break;
		}
		if ((dst_ip = libnet_name2addr4(l, optarg, LIBNET_RESOLVE)) == (u_int32_t)-1) {
			fprintf(stderr, "Bad dest IP\n");
			exit( -1 );
		}

		break;
	  case 'a':
		  if ((src_ip_b = libnet_name2addr4(l, optarg, LIBNET_RESOLVE)) == (u_int32_t)-1) {
			fprintf(stderr, "Bad source begain IP\n");
			exit( -1 );
		}

		  break;
	  case 'b':
		  if ((dst_ip_b = libnet_name2addr4(l, optarg, LIBNET_RESOLVE)) == (u_int32_t)-1) {
			fprintf(stderr, "Bad source begain IP\n");
			exit( -1 );
		}

		  break;
	  case 'y':
		  if ((src_ip_e = libnet_name2addr4(l, optarg, LIBNET_RESOLVE)) == (u_int32_t)-1) {
			fprintf(stderr, "Bad source begain IP\n");
			exit( -1 );
		}

		  break;
	  case 'z':
		  if ((dst_ip_e = libnet_name2addr4(l, optarg, LIBNET_RESOLVE)) == (u_int32_t)-1) {
			fprintf(stderr, "Bad source begain IP\n");
			exit( -1 );
		}

		  break;

	   case 's':
		src_port_rand = 1;
		if ( (tmp_port = index(optarg, ',')) != NULL ) {
			*tmp_port++ = '\0';
			src_port_rand = 0;
			src_prt = htons((u_int) atol(tmp_port));
		}
		if ( strcmp(optarg, "rand") == 0 ) {
			printf("Using random source IP's\n");
			src_ip = 1;	/* Just to pass sanity checks */
			src_ip_rand = 1;
			break;
		}
		if ((src_ip = libnet_name2addr4(l, optarg, LIBNET_RESOLVE)) == (u_int32_t)-1) {
			fprintf(stderr, "Bad source IP\n");
			exit( -1 );
		}
		break;
	   case 'l':
		payload_s = atoi(optarg);
		break;
	   case 'r':
		seed = atoi(optarg);
		break;
	   case 'm':
		max_pushed = atol(optarg);
		break;
	   case 'k':
		skip = atol(optarg);
		printf("Will not transmit first %li packets.\n", skip);
		break;
	   case 'D':
		printout++;
		break;
	   case 'p':
		num_to_send = atoi(optarg);
		break;
	   case 'V':
		BadIPVer = atof(optarg);
		break;
	   case 'F':
		FragPct = atof(optarg);
		break;
	   case 'I':
		IPOpts = atof(optarg);
		break;
	   case 'T':
		TCPOptsPct = atof(optarg);
		break;
	   case 'u':
		UrgPct = atof(optarg);
		break;
	   case 't':
		TCPCksm = atof(optarg);
		break;
	   case 'x':
		repeat = atoi(optarg);
		break;
	   case 'v':
		printf("Version %s\n", VERSION);
		exit(0);
	   }
	}
	if(src_ip_b && src_ip_e)
	{
		if(htonl(src_ip_e) > htonl(src_ip_b))
		{
			src_len = htonl(src_ip_e) - htonl(src_ip_b);
		}
		else
		{
			src_len = htonl(src_ip_b) - htonl(src_ip_e);
		}
	}
	if(dst_ip_b && dst_ip_e)
	{
		if(htonl(dst_ip_e) > htonl(dst_ip_b))
			dst_len = htonl(dst_ip_e) - htonl(dst_ip_b);
		else
			dst_len = htonl(dst_ip_b) - htonl(dst_ip_e);
	}


	if ( !src_ip || !dst_ip ) {
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	//max_pushed = 1;
	printf("Compiled against Libnet %s\n", LIBNET_VERSION);
	printf("Installing Signal Handlers.\n");
	if ( signal(SIGTERM, &sighandler) == SIG_ERR )
		printf("Failed to install signal handler for SIGTERM\n");
	if ( signal(SIGINT, &sighandler) == SIG_ERR )
		printf("Failed to install signal handler for SIGINT\n");
	if ( signal(SIGQUIT, &sighandler) == SIG_ERR )
		printf("Failed to install signal handler for SIGQUIT\n");

	printf("Seeding with %i\n", seed);
	srand(seed);
	max_pushed *= 1024;

	if ( (buf = malloc(IP_MAXPACKET)) == NULL ) {
		perror("malloc: ");
		exit( -1 );
	}


	if ( max_pushed >= 10000000 )
	 	printf("No Maximum traffic limiter\n");
	else printf("Maximum traffic rate = %.2f k/s\n", max_pushed/1024.0 );

	if ( src_port_rand )
	 	printf("Using random source ports.\n");
	if ( dst_port_rand )
	 	printf("Using random destination ports.\n");

	printf("Bad IP Version\t= %.0f%%\t\t", BadIPVer);
	printf("IP Opts Pcnt\t= %.0f%%\n", IPOpts);

	printf("Frag'd Pcnt\t= %.0f%%\t\t", FragPct);
	printf("Urg Pcnt\t= %.0f%%\n", UrgPct);

	printf("Bad TCP Cksm\t= %.0f%%\t\t", TCPCksm);
	printf("TCP Opts Pcnt\t= %.0f%%\n", TCPOptsPct);
	printf("\n");


	/* Drop them down to floats so we can multiply and not overflow */
	BadIPVer	/= 100;
	FragPct		/= 100;
	IPOpts		/= 100;
	UrgPct		/= 100;
	TCPOptsPct	/= 100;
	TCPCksm		/= 100;

    

	/*************
 	* Main Loop *
 	*************/
	gettimeofday(&tv, NULL);
	gettimeofday(&starttime, NULL);

	for(acx = 0; acx < num_to_send; acx++) {
		packet_len = IP_H;
	
		tos	= RAND8;
		id	= acx & 0xffff;
		ttl	= RAND8;


		if ( rand() <= (RAND_MAX * FragPct) )
			frag_off = RAND16;
		else	frag_off = 0;

		/* We're not going to pad IP Options */
		if ( rand() <= (RAND_MAX * IPOpts) ) {
			ipopt_len = 10 * (rand() / (float) RAND_MAX);
			ipopt_len = ipopt_len << 1;
			ip_opts = (u_short *) buf;
			packet_len += ipopt_len << 1;

			for ( cx = 0; cx < ipopt_len; cx++ )
				ip_opts[cx] = RAND16;
			tcp = (struct tcphdr *) (buf + IP_H + (ipopt_len << 1));
			ipopt_len = ipopt_len >> 1;
		} else {
			ipopt_len = 0;
			tcp = (struct tcphdr *) (buf + IP_H);
		}


		if ( src_ip_rand == 1 )
		{
			if(src_len)
			{
				src_ip = ntohl(htonl(src_ip_b) + RAND32 % src_len);
			}
			else
			{
				src_ip = RAND32;
			}
		}
		if ( dst_ip_rand == 1 )
		{
			if(dst_len)
				dst_ip = ntohl(htonl(dst_ip_b) + RAND32 % dst_len);
			else
				dst_ip = RAND32;
		}


		if ( rand() <= (RAND_MAX * BadIPVer ) )
			ver = rand() & 0xf;
		else	ver = 4;

		if (payload_s == 0)
			payload_s = rand() & 0x4ff;            /* length of 1279 */
		packet_len += payload_s;

		if ( rand() <= (RAND_MAX * TCPOptsPct) )
			tcp->th_off = rand() & 0xf;
		else	tcp->th_off = 5;
		packet_len += (tcp->th_off) << 2;
		

		/*
 		*  Build the IP header
 		*/
		ip_hdr = (struct ip *) buf;
		ip_hdr->ip_v    = ver;                 /* version 4 */
		ip_hdr->ip_hl   = 5 + ipopt_len;       /* 20 byte header */
		ip_hdr->ip_tos  = tos;                 /* IP tos */
		ip_hdr->ip_len  = htons(packet_len);   /* total length */
		ip_hdr->ip_id   = htons(id);           /* IP ID */
		ip_hdr->ip_off  = htons(frag_off);     /* fragmentation flags */
		ip_hdr->ip_ttl  = ttl;                 /* time to live */
		ip_hdr->ip_p    = IPPROTO_TCP;         /* transport protocol */
		ip_hdr->ip_sum  = 0;                   /* do this later */
		ip_hdr->ip_src.s_addr = src_ip;
		ip_hdr->ip_dst.s_addr = dst_ip;
		
		if ( src_port_rand == 1 )
			tcp->th_sport = RAND16;
		else	tcp->th_sport = src_prt;
		if ( dst_port_rand == 1 )
			tcp->th_dport = RAND16;
		else	tcp->th_dport = dst_prt;

		tcp->th_seq	= RAND32;
		tcp->th_ack	= RAND32;
		tcp->th_x2	= rand() & 0xf;		/* 4bit */
		tcp->th_flags	= RAND8;
		tcp->th_win	= RAND16;
		tcp->th_sum	= 0;
		if ( rand() <= (RAND_MAX * UrgPct) )
			tcp->th_urp = RAND16;
		else	tcp->th_urp = 0;


		payload = (u_short *)((u_char *) tcp + 20);
		for(cx = 0; cx <= (payload_s >> 1); cx+=1)
			payload[cx] = RAND16;

		if ( rand() <= (RAND_MAX * TCPCksm) )
			tcp->th_sum = RAND16;
		else	libnet_do_checksum(l, (u_int8_t *)buf, IPPROTO_TCP, (tcp->th_off<<2)+payload_s);
		
		if ( printout ) {
			printf("%s ->",
				inet_ntoa(*((struct in_addr*) &src_ip )));
			printf(" %s tos[%i] id[%i] ver[%i] frag[%i]\n",
				inet_ntoa(*((struct in_addr*) &dst_ip )), tos,
				id, ver, frag_off);
		}		
 		 
		if ( skip <= acx ) {
			for ( cx = 0; cx < repeat; cx++ ) {
				c = libnet_write_raw_ipv4(l, buf, packet_len);
				if (c != -1)
				  datapushed+=c;
			}
			if (c < (packet_len) ) {
				perror("Failed to send packet");
		/*   Reduce noise
		 *		fprintf(stderr, "Packet Failed '%i'\n", c);
		 *		printf("%s ->", 
		 *		     inet_ntoa(*((struct in_addr*) &src_ip )));
		 *
		 *		printf(" %s tos[%i] id[%i] ver[%i] "
		 *		     "frag[%i]\n",
		 *		     inet_ntoa(*((struct in_addr*) &dst_ip )),
		 *			tos, id, ver, frag_off);
		 */
		 	}
		} 

		if ( !(acx % 1000) ) {
			if ( acx == 0 )
				continue;
			gettimeofday(&tv2, NULL);
			sec = (tv2.tv_sec - tv.tv_sec)
			      - (tv.tv_usec - tv2.tv_usec) / 1000000.0;
			printf(" %li @ %.1f pkts/sec and %.1f k/s\n", acx,
				1000/sec, (datapushed / 1024.0) / sec);
			datapushed=0;
			gettimeofday(&tv, NULL);
		}

		/* Flood protection for low traffic only. */
		if ( max_pushed < 10000000 ) {
			gettimeofday(&tv2, NULL);
			sec = (tv2.tv_sec - tv.tv_sec)
		      		- (tv.tv_usec - tv2.tv_usec) / 1000000.0;
#if 1
			if ( (datapushed / sec) >= max_pushed )
			{
				//printf("this to sleep \n");
				usleep(10);	/* 10 should give up our timeslice */
			}
#endif
		}
	}


	gettimeofday(&tv, NULL);
	printf("\nWrote %li packets in %.2fs @ %.2f pkts/s\n", acx,
		(tv.tv_sec-starttime.tv_sec)
		+ (tv.tv_usec-starttime.tv_usec) / 1000000.0,
		acx / ((tv.tv_sec-starttime.tv_sec)
                       + (tv.tv_usec-starttime.tv_usec)/1000000.0) );

	libnet_destroy(l);
	free(buf);
	return ( 0 );
}


void usage(char *name)
{
   fprintf(stderr,
	"usage: %s [-v] [-D] -s <sourceip>[,port] -d <destination ip>[,port]\n"
	"          [-r seed] [-m <max kB/s to generate>] [-l length per package]\n"
	"          [-p <pkts to generate>] [-k <skip packets>] [-x <repeat times>]\n\n"
	"       Percentage Opts: [-F frags] [-V <Bad IP Version>] [-I <IP Options>]\n"
	"                        [-T <TCP Options>] [-u <urgent data>] [-t <Bad TCP Cksm>]\n\n"
	"       [-D] causes packet info to be printed out -- DEBUGGING\n\n"
	"       ex: -s 10.10.10.10,23   -d 10.10.10.100 -I 100\n"
	"           will give a 100%% chance of IP Options ^^^\n"
	"       ex: -s 10.10.10.10,23   -d 10.10.10.100 -p 100 -r 103334\n"
	"       ex: -s rand -d rand,1234 -r 23342\n"
	"              ^^^^ causes random source addr\n"
	"       ex: -s rand -d rand -k 10000 -p 10001 -r 666\n"
	"           Will only send the 10001 packet with random seed 666\n"
	"           this is especially useful if you suspect that packet is\n"
	"           causing a problem with the target stack.\n\n",
	((char *) rindex(name, '/')) == ((char *) NULL)
		? (char *) name
		: (char *) rindex(name, '/') + 1);
}

void sighandler(int sig)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);

	printf("\n");
	printf("Caught signal %i\n", sig);

	printf("Used random seed %i\n", seed);
	printf("Wrote %li packets in %.2fs @ %.2f pkts/s\n", acx,
		(tv.tv_sec - starttime.tv_sec)
		  + (tv.tv_usec - starttime.tv_usec)/1000000.0,
		acx / (( tv.tv_sec - starttime.tv_sec)
		  + (tv.tv_usec - starttime.tv_usec)/1000000.0)
		);

	fflush(stdout);
	exit(0);
}
