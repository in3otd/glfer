/* qrs.c
 * 
 * Copyright (C) 2001 Claudio Girardi
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <ctype.h>		/* for toupper() */

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include <signal.h>
#include <sys/kd.h>		/* for KIOCSOUND */
#include <fcntl.h>		/* file control definitions */
#include <sys/ioctl.h>
/* #include <asm/io.h>  *//* for inb() and outb() in libc5 only ? */
#include "qrs.h"
#include "glfer.h"
#include "util.h"

#include <string.h>		/* string function definitions */

/* ioperm is in unistd.h  in libc5 */
#include <unistd.h>		/* UNIX standard function definitions */
/* ioperm is in sys/io.h in glibc */
/* conditionals for architectures without sys/io.h added by Joop Stakenborg */
#if HAVE_SYS_IO_H
# include <sys/io.h>
#endif

#include <errno.h>		/* error number definitions */
#include <termios.h>		/* POSIX terminal control definitions */

#ifdef HAVE_DMALLOC_H
#include <dmalloc.h>		/* dmalloc.h should be included last */
#endif


#define PP_BIT0  1		/* parallel port DATA0 line */
#define PP_BIT1  2		/* parallel port DATA1 line */
#define PP_PTT PP_BIT1
#define PP_KEY PP_BIT0
#define PP_BASE0  0x378		/* printer port 0 base address */
#define PP_BASE1  0x278		/* printer port 1 base address */


extern opt_t opt;

static int ksound_fd = -1;	/* console file descriptor */
static int serial_fd = -1;	/* serial port file descriptor */
static int ksound_opened = FALSE;
static int repeat_msg = FALSE; /* repeat message transmission (beacon mode) */


/* function prototypes */
static void send_next_element(int signal);
static void set_alarm_timer(float sec);
void send_next_char(int trig_signal);
static void emit_sound(float freq);
/* morse code characters table */
static struct
{
  char key;
  char *cw;
}
cw_chars[] =
{
  { 'A', ".-"}, 
  { 'B', "-..."},
  { 'C', "-.-."},
  { 'D', "-.."},
  { 'E', "."},
  { 'F', "..-."},
  { 'G', "--."},
  { 'H', "...."},
  { 'I', ".."},
  { 'J', ".---"},
  { 'K', "-.-"},
  { 'L', ".-.."},
  { 'M', "--"},
  { 'N', "-."},
  { 'O', "---"},
  { 'P', ".--."},
  { 'Q', "--.-"},
  { 'R', ".-."},
  { 'S', "..."},
  { 'T', "-"},
  { 'U', "..-"},
  { 'V', "...-"},
  { 'W', ".--"},
  { 'X', "-..-"},
  { 'Y', "-.--"},
  { 'Z', "--.."},
  { '0', "-----"},
  { '1', ".----"},
  { '2', "..---"},
  { '3', "...--"},
  { '4', "....-"},
  { '5', "....."},
  { '6', "-...."},
  { '7', "--..."},
  { '8', "---.."},
  { '9', "----."},
  { '?', "..--.."},
  { '/', "-..-."},
  { '.', ".-.-.-"},
  { '@', ".-.-."},				/* AR */
  { '$', "...-.-"},				/* SK */
  { '%', "-...-.-"},				/* BK */
  { '*', "-.-.-"} ,				/* CT */
/*{ '(', "-.--." },
  { ')', "-.--.-" },
  { '"', ".-..-." },
  { '\'', ".----." },
  { ',', "--..--" },
  { '-', "-....-" },
  { ':', "---..." },
  { ';', "-.-.-." },
  { '=', "-...-" },
  { '_', "..--.-" },*/
  { ' ', " "},
  { 0, ""}			/* end of CW char list */
};


char *s;
static int str_index;		/* string index */
static int str_len;		/* string length */
static int cw_char_index;	/* cw char string index */
static int tab_index;		/* cw lookup table index */
static int cw_char_len;		/* cw char string length */


static void set_alarm_timer(float msec)
{
  struct itimerval element_time;
  long sec, usec;

  sec = floor(msec / 1000.0);
  usec = 1e03 * (msec - 1000.0 * sec);

  element_time.it_interval.tv_sec = 0;
  element_time.it_interval.tv_usec = 0;
  element_time.it_value.tv_sec = sec;
  element_time.it_value.tv_usec = usec;

  setitimer(ITIMER_REAL, &element_time, NULL);
}


static void emit_sound(float freq)
{
  int kparam;

  /* sanity check */
  if (ksound_opened == TRUE) {
    /* parameter for KIOCSOUND ioctl for setting the frequency */
    kparam = (freq != 0.0 ? (1193180 / freq) : 0);

    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    if (ioctl(ksound_fd, KIOCSOUND, kparam) == -1)
      perror("emit_sound(): KIOCSOUND ioctl returned -1");
    signal(SIGINT, SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
  }
}


int open_parport(char *device)
{
#if HAVE_SYS_IO_H
  uid_t real_uid;
  gid_t real_gid;

  /* checking I/O permissions */
  if (ioperm(PP_BASE0, 3, 1)) {
    /* can not access the parallel port */
    fprintf(stderr, "Error: Couldn't get the parallel port at %x\n", PP_BASE0);
    fprintf(stderr, "You need root privileges to run this program.\n");
    return -1;
  } else {
    /* OK, now drop root privileges */
    real_uid = getuid();
    setuid(real_uid);
    real_gid = getgid();
    setgid(real_gid);
    /* make sure PTT and KEY are off */
    ptt_off();
    key_up();
  }
#endif
  return 0;
}


/* open_serial_port() - open serial port
 *
 * returns the file descriptor on success or -1 on error
 */
int open_serial_port(char *device)
{
  struct termios options;
  int status;
  char *dev_full_name;
  char error_string[] = "open_serial_port: unable to open ";
  char *error_message;

  /* allocate enough space for the complete device name */
  dev_full_name = malloc((strlen(device) + 6) * sizeof(char));
  /* it will be in "/dev" */
  strcpy(dev_full_name, "/dev/");
  /* add the device name */
  strcat(dev_full_name, device);

  /* try to open the device */
  serial_fd = open(dev_full_name, O_RDWR | O_NOCTTY | O_NDELAY);

  if (serial_fd == -1) {
    /* could not open the port */
    error_message = (char *) malloc((strlen(error_string) + strlen(dev_full_name) + 1) * sizeof(char));
    strcpy(error_message, error_string);
    strcat(error_message, dev_full_name);
    perror(error_message);
    free(dev_full_name);
    free(error_message);
    return -1;
  } else {
    /* set normal behaviour */
    fcntl(serial_fd, F_SETFL, 0);
  }

  /* get the current options */
  if (tcgetattr(serial_fd, &options) < 0) {
    fprintf(stderr, "open_serial_port: error in tcgetattr\n");
    return -1;
  }

  /* set raw input, 1 second timeout  */
  options.c_iflag = IGNBRK;
  options.c_cflag |= (CLOCAL | CREAD);
  options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
  options.c_oflag &= ~OPOST;
  options.c_cc[VMIN] = 0;
  options.c_cc[VTIME] = 10;

  /* set the options */
  if (tcsetattr(serial_fd, TCSANOW, &options) < 0) {
    fprintf(stderr, "open_serial_port: error in tcsetattr\n");
    return -1;
  }

  /* get serial port current status */
  ioctl(serial_fd, TIOCMGET, &status);
  /* clear DTR and RTS */
  status &= ~(TIOCM_DTR | TIOCM_RTS);
  ioctl(serial_fd, TIOCMSET, &status);
  /* device name is not used anynmore here */
  free(dev_full_name);

  return (serial_fd);
}


void close_serial_port(void)
{
  if (serial_fd != -1) {	/* port opened */
    if (close(serial_fd) == -1) {
      /* an error occurred */
      perror("close_serial_port");
    } else {
      /* serial port released */
      serial_fd = -1;
    }
  }
}


void ptt_on(void)
{
  int status;
#if HAVE_SYS_IO_H
  unsigned short pp_status;	/* parallel port status */
#endif

  switch (opt.device_type) {
  case DEV_SERIAL:
    /* get serial port current status */
    ioctl(serial_fd, TIOCMGET, &status);
    /* set DTR */
    status |= TIOCM_DTR;
    ioctl(serial_fd, TIOCMSET, &status);
    break;
#if HAVE_SYS_IO_H
  case DEV_PARALLEL:
    pp_status = inb(PP_BASE0);
    pp_status |= PP_BIT1;
    outb((unsigned char) pp_status, PP_BASE0);
    break;
#endif
  default:
    fprintf(stderr, "ptt_on: invalid opt.device_type\n");
    break;
  }
  D(printf("PTT ON\n"));	/* for debug */
}


void ptt_off(void)
{
  int status;
#if HAVE_SYS_IO_H
  unsigned short pp_status;	/* parallel port status */
#endif

  switch (opt.device_type) {
  case DEV_SERIAL:
    /* get serial port current status */
    ioctl(serial_fd, TIOCMGET, &status);
    /* clear DTR */
    status &= ~TIOCM_DTR;
    ioctl(serial_fd, TIOCMSET, &status);
    break;
#if HAVE_SYS_IO_H
  case DEV_PARALLEL:
    pp_status = inb(PP_BASE0);
    pp_status &= ~PP_BIT1;
    outb((unsigned char) pp_status, PP_BASE0);
    break;
#endif
  default:
    fprintf(stderr, "ptt_off: invalid opt.device_type\n");
    break;
  }
  D(printf("PTT OFF\n"));	/* for debug */
}


void key_down(void)
{
  int status;
#if HAVE_SYS_IO_H
  unsigned short pp_status;	/* parallel port status */
#endif

  switch (opt.device_type) {
  case DEV_SERIAL:
    /* get serial port current status */
    ioctl(serial_fd, TIOCMGET, &status);
    /* set RTS */
    status |= TIOCM_RTS;
    ioctl(serial_fd, TIOCMSET, &status);
    break;
#if HAVE_SYS_IO_H
  case DEV_PARALLEL:
    pp_status = inb(PP_BASE0);
    pp_status |= PP_BIT0;
    outb((unsigned char) pp_status, PP_BASE0);
    break;
#endif
  default:
    fprintf(stderr, "key_down: invalid opt.device_type\n");
    break;
  }
  D(printf("KEY DOWN\n"));	/* for debug */
}


void key_up(void)
{
  int status;
#if HAVE_SYS_IO_H
  unsigned short pp_status;	/* parallel port status */
#endif

  switch (opt.device_type) {
  case DEV_SERIAL:
    /* get serial port current status */
    ioctl(serial_fd, TIOCMGET, &status);
    /* clear RTS */
    status &= ~TIOCM_RTS;
    ioctl(serial_fd, TIOCMSET, &status);
    break;
#if HAVE_SYS_IO_H
  case DEV_PARALLEL:
    pp_status = inb(PP_BASE0);
    pp_status &= ~PP_BIT0;
    outb((unsigned char) pp_status, PP_BASE0);
    break;
#endif
  default:
    fprintf(stderr, "key_up: invalid opt.device_type\n");
    break;
  }
  D(printf("KEY UP\n"));	/* for debug */
}


static void dfcw_dot_pause(int trig_signal)
{
  if (cw_char_index < (cw_char_len - 1)) {
    /* not at the end of the char */
    cw_char_index++;
    /* if previous element was a dot insert a gap */
    if (cw_chars[tab_index].cw[cw_char_index - 1] == '.') {
      /* stop sidetone */
      emit_sound(0);
      /* TX off */
      key_up();
      set_alarm_timer(opt.dfcw_gap_time);
      signal(SIGALRM, send_next_element);
    } else {
      /* otherwise make no pause and start sending next char */
      send_next_element(SIGALRM);
    }
  } else if (str_index < (str_len - 1)) {
    /* stop sidetone */
    emit_sound(0);
    /* TX off */
    key_up();
    /* not at the end of the string to be transmitted */
    str_index++;
    D(printf("<next char>, str_index = %i\n", str_index));	/* for debug */
    /* pause between chars */
    set_alarm_timer(3.0 * opt.dot_time);
    signal(SIGALRM, send_next_char);
  } else {
    /* stop sidetone */
    emit_sound(0);
    /* end of transmission */
    key_up();
    /* release PTT (not needed) */
    ptt_off();
  }
}


static void qrss_dot_pause(int trig_signal)
{
  /* switch off emission */
  key_up();
  /* stop sidetone */
  emit_sound(0);

  if (cw_char_index < (cw_char_len - 1)) {
    /* not at the end of the char */
    cw_char_index++;
    /* one dot pause between chars */
    signal(SIGALRM, send_next_element);
    set_alarm_timer(opt.dot_time);
  } else if (str_index < (str_len - 1)) {
    /* not at the end of the string to be transmitted */
    str_index++;
    D(printf("<next char>, str_index = %i\n", str_index));	/* for debug */
    /* pause between chars */
    signal(SIGALRM, send_next_char);
    set_alarm_timer(5.0 * opt.dot_time);
  } else if (str_index < str_len) {
    /* end of the string, end of transmission */
    if (repeat_msg == FALSE) { /* not in beacon mode */
      str_index++; /* signal that last character was sent */
      signal(SIGALRM, qrss_dot_pause);
      set_alarm_timer(opt.ptt_delay);
    } else { /* in beacon mode */
      if (opt.beacon_tx_pause == TRUE) {
	key_down();
	if (opt.sidetone == TRUE)
	  emit_sound(opt.sidetone_freq);
      }
      str_index = 0; /* reset pointer to beginning of string */
      signal(SIGALRM, send_next_char);
      /* start transmissions after  */
      set_alarm_timer(opt.beacon_pause * 1000); /* pause is in seconds */
    }
  } else { /* not in beacon mode */
    ptt_off(); /* transmission ends here */
  }
}


static void send_dot(void)
{
  switch (opt.tx_mode) {
  case QRSS:
    /* make sure the TX is off */
    key_up();
    /* stop sidetone */
    emit_sound(0);
    signal(SIGALRM, qrss_dot_pause);
    set_alarm_timer(opt.dot_time);
    D(putchar('.');
      fflush(stdout));		/* for debug */
    if (opt.sidetone == TRUE)
      emit_sound(opt.sidetone_freq);
    key_down();
    break;
  case DFCW:
    key_down();
    /* "normal" DFCW has DTR=high for dash */
    ptt_on();
    if (opt.sidetone == TRUE)
      emit_sound(opt.dfcw_dot_freq);
    signal(SIGALRM, dfcw_dot_pause);
    set_alarm_timer(opt.dot_time - opt.dfcw_gap_time);
    break;
  default:
    break;
  }
}


static void send_dash(void)
{
  switch (opt.tx_mode) {
  case QRSS:
    /* make sure the TX is off */
    key_up();
    /* stop sidetone */
    emit_sound(0);
    signal(SIGALRM, qrss_dot_pause);
    set_alarm_timer(opt.dash_dot_ratio * opt.dot_time);
    D(putchar('-');
      fflush(stdout);
      )				/* for debug */
      if (opt.sidetone == TRUE)
      emit_sound(opt.sidetone_freq);
    key_down();
    break;
  case DFCW:
    key_down();
    /* "normal" DFCW has DTR=low for dot */
    ptt_off();
    if (opt.sidetone == TRUE)
      emit_sound(opt.dfcw_dash_freq);
    signal(SIGALRM, dfcw_dot_pause);
    set_alarm_timer(opt.dot_time);
    break;
  default:
    break;
  }
}


static void send_space(void)
{
  switch (opt.tx_mode) {
  case QRSS:
    signal(SIGALRM, qrss_dot_pause);
    set_alarm_timer(5.0 * opt.dot_time);
    D(putchar(' ');
      fflush(stdout));		/* for debug */
    break;
  case DFCW:
    signal(SIGALRM, dfcw_dot_pause);
    set_alarm_timer(3.0 * opt.dot_time);
    D(putchar(' ');
      fflush(stdout));		/* for debug */
    break;
  default:
    break;
  }
}


static void send_next_element(int trig_signal)
{
  /* signal type is ignored, only SIGALRM should trigger this function */
  /* send a dot, dash or space */
  if (cw_chars[tab_index].cw[cw_char_index] == '-') {
    send_dash();
  } else if (cw_chars[tab_index].cw[cw_char_index] == '.') {
    send_dot();
  } else {			/* is space */
    send_space();
  }
  //key_down();
}


/*
 * send_char - routine to send a given ASCII character as cw, and echo
 *             to stdout, and optionally wait for the post-character gap
 */
void send_next_char(int trig_signal)
{
  char c;

  c = s[str_index];
  /* ensure uppercase */
  c = toupper(c);

  /* check table to find character to send */
  tab_index = 0;
  while (cw_chars[tab_index].key != 0) {
    if (cw_chars[tab_index].key == c) {
      /* if found, send character */
      /* echo the character */
      D(putchar(c);
	putchar(':');
	putchar(' ');
	fflush(stdout));	/* for debug */
      /* sound the elements of the cw equivalent */
      cw_char_len = strlen(cw_chars[tab_index].cw);
      cw_char_index = 0;
      send_next_element(SIGALRM);
      return;
    } else {
      tab_index++;
    }
  }
  /* not found - print an error message */
  fprintf(stderr, "send_next_char: char not recognized\n");
}


int cw_char_allowed(char c)
{
  /* ensure uppercase */
  c = toupper(c);

  /* scan table to find character */
  tab_index = 0;
  while (cw_chars[tab_index].key != 0) {
    if (cw_chars[tab_index].key == c) {
      /* found, character is allowed */
      return TRUE;
    } else {
      tab_index++;
    }
  }
  /* not found - character is not allowed */
  return FALSE;
}


int get_qrss_char_index(void)
{
  return str_index;
}

/*
 * send_string - routine to send a given ASCII string as cw, and echo
 *               to stdout; can also send multi-char items
 */
void send_string(char *str)
{
  s = str;
  /* simply send every character in the string */
  str_index = 0;
  str_len = strlen(s);

  repeat_msg = opt.beacon_mode;

  switch (opt.tx_mode) {
  case QRSS:
    /* close PTT */
    ptt_on();
    signal(SIGALRM, send_next_char);
    /* start transmissions after ptt_delay */
    set_alarm_timer(opt.ptt_delay);
    break;
  case DFCW:
    /* close KEY */
    key_down();
    signal(SIGALRM, send_next_char);
    /* start transmissions after ptt_delay */
    set_alarm_timer(opt.ptt_delay);
    break;
  default:
    break;
  }
}


void stop_tx(void)
{
  /* avoid restart of message transmission */
  repeat_msg = FALSE;
  /* stop sidetone */
  emit_sound(0);
  /* hack, pretend we are at the end of the string */
  cw_char_index = cw_char_len;
  str_index = str_len - 1;

  switch (opt.tx_mode) {
  case QRSS:
    /* switch off TX */
    key_up();
    signal(SIGALRM, qrss_dot_pause);
    break;
  case DFCW:
    signal(SIGALRM, dfcw_dot_pause);
    break;
  default:
    break;
  }
  /* schedule next event to be (almost) now */
  set_alarm_timer(1e-03);
}


float string_duration(char *str)
{
  int i, j, k;
  char c;
  float overall_time = 0.0;

  /* add the PTT delay at start and end of transmissions */
  overall_time += 2.0 * opt.ptt_delay;

  for (i = 0; i < strlen(str); i++) {
    /* ensure uppercase */
    c = toupper(str[i]);
    /* scan table to find character */
    for (j = 0; cw_chars[j].key != 0; j++)
      if (cw_chars[j].key == c) {
	for (k = 0; k < strlen(cw_chars[j].cw); k++) {
	  if (cw_chars[j].cw[k] == '-') {
	    /* dash duration; different between QRSS and DFCW */
	    switch (opt.tx_mode) {
	    case QRSS:
	      overall_time += opt.dash_dot_ratio * opt.dot_time;
	      break;
	    case DFCW:
	      overall_time += opt.dot_time;
	      break;
	    default:
	      break;
	    }
	  } else if (cw_chars[j].cw[k] == '.') {
	    /* dot duration */
	    overall_time += opt.dot_time;
	  } else {
	    /* space duration; different between QRSS and DFCW */
	    switch (opt.tx_mode) {
	    case QRSS:
	      overall_time += 5.0 * opt.dot_time;
	      break;
	    case DFCW:
	      overall_time += 3.0 * opt.dot_time;
	      break;
	    default:
	      break;
	    }
	  }
	  if (k < (strlen(cw_chars[j].cw) - 1)) {
	    /* not at end of char */
	    /* space between elements */
	    switch (opt.tx_mode) {
	    case QRSS:
	      overall_time += opt.dot_time;
	      break;
	    case DFCW:
	      /* no space between elements */
	      break;
	    default:
	      break;
	    }
	  } else if (i < (strlen(str) - 1)) {
	    /* at end of char but not at end of string */
	    /* space between chars */
	    switch (opt.tx_mode) {
	    case QRSS:
	      overall_time += 5.0 * opt.dot_time;
	      break;
	    case DFCW:
	      overall_time += 3.0 * opt.dot_time;
	      break;
	    default:
	      break;
	    }
	  }			/* else string is finished */
	}
      }
  }
  return overall_time;
}


int open_ksound(void)
{
  /* To be used as the ksound_fd in ioctl(). */
  if ((ksound_fd = open("/dev/console", O_NOCTTY)) == -1) {
    perror("open_ksound: unable to open /dev/console");
    return (ksound_opened = FALSE);
  } else
    return (ksound_opened = TRUE);
}


void close_ksound(void)
{
  if (ksound_opened == TRUE)
    close(ksound_fd);
}
