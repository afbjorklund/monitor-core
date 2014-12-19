#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <fcntl.h>
#include <time.h>

#include "ganglia_gexec.h"
#include "ganglia_priv.h"
#include "llist.h"
#include <getopt.h>
#include "cmdline.h"

char cluster_ip[16];
unsigned short cluster_port;

struct gengetopt_args_info args_info;

void process_opts( int argc, char **argv );

static int debug_level;

int main(int argc, char *argv[])
{
   int rval;
   gexec_cluster_t cluster;
   gexec_host_t *host;
   llist_entry *li;
   int fd;
   struct stat st;
   char *tmp;
   ssize_t n;
   int num_hosts;
   char *distcc;
   char *hosts;
   char *p;
   int status;

   debug_level = 1;
   set_debug_msg_level(debug_level);

   if (cmdline_parser (argc, argv, &args_info) != 0)
      exit(EXIT_FAILURE) ;

#if 0
   fd = shm_open("/DISTCC_HOSTS", O_RDONLY, 0444);
   if (fd != -1 && 0) {
        hosts = mmap(NULL, 1024, PROT_READ, MAP_SHARED, fd, 0);
        if (hosts)
            printf("shm: %s\n", hosts);
   } 
#else
   fd = open("/dev/shm/gdistcc", O_RDONLY, 0444);
   if (fd != -1) {
      fstat(fd, &st);
      if (st.st_mtime + args_info.seconds_arg > time(NULL)) {
          hosts = calloc(1024, 1);
          n = read(fd, hosts, 1024);
          close(fd);
          if (*hosts)
               goto hosts;
      }
   }
#endif

   rval = gexec_cluster(&cluster, args_info.gmond_ip_arg, args_info.gmond_port_arg );
   if ( rval != 0)
      {
         fprintf(stderr, "Unable to get hostlist from %s %d!\n", args_info.gmond_ip_arg, args_info.gmond_port_arg);
         goto distcc;
      }

   if( args_info.debug_flag )
      {
         printf("CLUSTER INFORMATION\n");
         printf("       Name: %s\n", cluster.name);
         printf("      Hosts: %d\n", cluster.num_hosts);
         printf(" Dead Hosts: %d\n", cluster.num_dead_hosts);
         printf("  Localtime: %s\n", ctime(&(cluster.localtime)) );
      }

      {
         li = cluster.hosts;
         if(! cluster.num_hosts )
            {
               fprintf(stderr, "There are no hosts up at this time\n"); 
               goto distcc;
            }
      }

   if( args_info.debug_flag)
      {
         printf("CLUSTER HOSTS\n");
         printf("Hostname                     LOAD                       CPU              \n");
         printf(" CPUs (Procs/Total) [     1,     5, 15min] [  User,  Nice, System, Idle,   Wio]\n\n");
      }
   num_hosts = 0;
   for(; li != NULL; li = li->next)
      {
         host = li->val;
         if( args_info.debug_flag)
         {
         printf("%s\n", host->ip);

         printf(" %4d (%5d/%5d) [%6.2f,%6.2f,%6.2f] [%6.1f,%6.1f,%6.1f,%6.1f,%6.1f]\n\n",
               host->cpu_num, host->proc_run, host->proc_total,
               host->load_one, host->load_five, host->load_fifteen,
               host->cpu_user, host->cpu_nice, host->cpu_system, host->cpu_idle, host->cpu_wio);
         }

         if (host->load_one < (float) host->cpu_num)
              num_hosts++;
      }

    if (num_hosts)
      {
    hosts = (char *) malloc(13 + cluster.num_hosts * 69 + 1);
    strcpy(hosts, "DISTCC_HOSTS="); 
    p = hosts + 13;    
         for( li = cluster.hosts; li != NULL; li = li->next )
            {
               host = li->val;
               if (host->load_one < (float) host->cpu_num) {
                   if (li != cluster.hosts)
                       *p++ = ' ';
                   p += sprintf(p, "%s/%d", host->ip, host->cpu_num);
                }
            }

   gexec_cluster_free(&cluster);

#if 0
    fd = shm_open("/DISTCC_HOSTS", O_CREAT, 0644);
    if (fd != -1) {
        (void) ftruncate(fd, strlen(hosts));
        p = mmap(NULL, strlen(hosts), PROT_WRITE, MAP_SHARED, fd, 0);
        if (p)
            strcpy(p, hosts);
        munmap(p, strlen(hosts));
    }
#else
   tmp = strdup("/dev/shm/gdistcc.XXXXXX");
   fd = mkstemp(tmp);
   if (fd != -1) {
      n = write(fd, hosts, strlen(hosts));
      if (n < 0)
          fprintf(stderr, "%s\n", strerror(errno));
      fchmod(fd, 0644);
      close(fd);
      rename(tmp, "/dev/shm/gdistcc");
   }
#endif

hosts:
    if (args_info.debug_flag)
        printf("%s\n", hosts);
    if (putenv((char *) hosts) != 0) {
	fprintf(stderr, "Error putting DISTCC_HOSTS in the environment\n");
	return -1;
    }
   }

distcc:
    if (args_info.verbose_flag)
        putenv("DISTCC_VERBOSE=1");
    if (optind < argc) {
    rval = fork();
    if (rval == 0) {
        if (args_info.executable_given)
            distcc = args_info.executable_arg;
        else {
            distcc = getenv("DISTCC");
            if (!distcc)
                distcc="distcc";
        }

	if (execvp(distcc, &argv[optind]) < 0) {
	    fprintf(stderr, "execvp failed: err %s\n", strerror(errno));
	    return -1;
	}
	return 0;
    } else if (rval < 0) {
	fprintf(stderr, "Failed to fork a process!\n");
	return -1;
    }

    status = 0;
    (void) wait(&status);
   }

   return 0;
}

