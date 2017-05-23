/*
 * testcode/unitauth.c - unit test for authzone authoritative zone code.
 *
 * Copyright (c) 2017, NLnet Labs. All rights reserved.
 *
 * This software is open source.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * Neither the name of the NLNET LABS nor the names of its contributors may
 * be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
/**
 * \file
 * Unit test for auth zone code.
 */
#include "config.h"
#include "services/authzone.h"
#include "testcode/unitmain.h"
#include "util/regional.h"
#include "util/net_help.h"
#include "util/data/msgreply.h"
#include "services/cache/dns.h"
#include "sldns/str2wire.h"
#include "sldns/wire2str.h"
#include "sldns/sbuffer.h"

/** verbosity for this test */
static int vbmp = 1;

/** struct for query and answer checks */
struct q_ans {
	/** zone to query (delegpt) */
	const char* zone;
	/** query name, class, type */
	const char* query;
	/** additional flags or "" */
	const char* flags;
	/** expected answer to check against, multi-line string */
	const char* answer;
};

/** auth zone for test */
static const char* zone_example_com =
"example.com.	3600	IN	SOA	ns.example.org. noc.example.org. 2017042710 7200 3600 1209600 3600\n"
"example.com.	3600	IN	A	10.0.0.1\n"
"example.com.	3600	IN	NS	ns.example.com.\n"
"example.com.	3600	IN	MX	50 mail.example.com.\n"
"deep.ent.example.com.	3600	IN	A	10.0.0.9\n"
"mail.example.com.	3600	IN	A	10.0.0.4\n"
"ns.example.com.	3600	IN	A	10.0.0.5\n"
"out.example.com.	3600	IN	CNAME	www.example.com.\n"
"plan.example.com.	3600	IN	CNAME	nonexist.example.com.\n"
"redir.example.com.	3600	IN	DNAME	redir.example.org.\n"
"sub.example.com.	3600	IN	NS	ns1.sub.example.com.\n"
"sub.example.com.	3600	IN	NS	ns2.sub.example.com.\n"
"ns1.sub.example.com.	3600	IN	A	10.0.0.6\n"
"ns2.sub.example.com.	3600	IN	AAAA	2001::7\n"
"*.wild.example.com.	3600	IN	A	10.0.0.8\n"
"www.example.com.	3600	IN	A	10.0.0.2\n"
"www.example.com.	3600	IN	A	10.0.0.3\n"
"yy.example.com.	3600	IN	TXT	\"a\"\n"
"yy.example.com.	3600	IN	TXT	\"b\"\n"
"yy.example.com.	3600	IN	TXT	\"c\"\n"
"yy.example.com.	3600	IN	TXT	\"d\"\n"
"yy.example.com.	3600	IN	TXT	\"e\"\n"
"yy.example.com.	3600	IN	TXT	\"f\"\n"

/* and some tests for RRSIGs (rrsig is www.nlnetlabs.nl copy) */
/* normal: domain and 1 rrsig */
"z1.example.com.	3600	IN	A	10.0.0.10\n"
"z1.example.com.	3600	IN	RRSIG	A 8 3 10200 20170612005010 20170515005010 42393 nlnetlabs.nl. NhEDrHkuIgHkjWhDRVsGOIJWZpSs+QdduilWFe5d+/ZhOheLJbaTYD5w6+ZZ3yPh1tNud+jlg+GyiOSVapLEO31swDCIarL1UfRjRSpxxDCHGag5Zu+S4hF+KURxO3cJk8jLBELMQyRuMRHoKrw/wsiLGVu1YpAyAPPMcjFBNbk= ;{id = 42393}\n"
/* normal: domain and 2 rrsigs */
"z2.example.com.	3600	IN	A	10.0.0.10\n"
"z2.example.com.	3600	IN	RRSIG	A 8 3 10200 20170612005010 20170515005010 42393 nlnetlabs.nl. NhEDrHkuIgHkjWhDRVsGOIJWZpSs+QdduilWFe5d+/ZhOheLJbaTYD5w6+ZZ3yPh1tNud+jlg+GyiOSVapLEO31swDCIarL1UfRjRSpxxDCHGag5Zu+S4hF+KURxO3cJk8jLBELMQyRuMRHoKrw/wsiLGVu1YpAyAPPMcjFBNbk= ;{id = 42393}\n"
"z2.example.com.	3600	IN	RRSIG	A 8 3 10200 20170612005010 20170515005010 12345 nlnetlabs.nl. NhEDrHkuIgHkjWhDRVsGOIJWZpSs+QdduilWFe5d+/ZhOheLJbaTYD5w6+ZZ3yPh1tNud+jlg+GyiOSVapLEO31swDCIarL1UfRjRSpxxDCHGag5Zu+S4hF+KURxO3cJk8jLBELMQyRuMRHoKrw/wsiLGVu1YpAyAPPMcjFBNbk= ;{id = 12345}\n"
/* normal: domain and 3 rrsigs */
"z3.example.com.	3600	IN	A	10.0.0.10\n"
"z3.example.com.	3600	IN	A	10.0.0.11\n"
"z3.example.com.	3600	IN	RRSIG	A 8 3 10200 20170612005010 20170515005010 42393 nlnetlabs.nl. NhEDrHkuIgHkjWhDRVsGOIJWZpSs+QdduilWFe5d+/ZhOheLJbaTYD5w6+ZZ3yPh1tNud+jlg+GyiOSVapLEO31swDCIarL1UfRjRSpxxDCHGag5Zu+S4hF+KURxO3cJk8jLBELMQyRuMRHoKrw/wsiLGVu1YpAyAPPMcjFBNbk= ;{id = 42393}\n"
"z3.example.com.	3600	IN	RRSIG	A 8 3 10200 20170612005010 20170515005010 12345 nlnetlabs.nl. NhEDrHkuIgHkjWhDRVsGOIJWZpSs+QdduilWFe5d+/ZhOheLJbaTYD5w6+ZZ3yPh1tNud+jlg+GyiOSVapLEO31swDCIarL1UfRjRSpxxDCHGag5Zu+S4hF+KURxO3cJk8jLBELMQyRuMRHoKrw/wsiLGVu1YpAyAPPMcjFBNbk= ;{id = 12345}\n"
"z3.example.com.	3600	IN	RRSIG	A 8 3 10200 20170612005010 20170515005010 12356 nlnetlabs.nl. NhEDrHkuIgHkjWhDRVsGOIJWZpSs+QdduilWFe5d+/ZhOheLJbaTYD5w6+ZZ3yPh1tNud+jlg+GyiOSVapLEO31swDCIarL1UfRjRSpxxDCHGag5Zu+S4hF+KURxO3cJk8jLBELMQyRuMRHoKrw/wsiLGVu1YpAyAPPMcjFBNbk= ;{id = 12356}\n"
/* just an RRSIG rrset with nothing else */
"z4.example.com.	3600	IN	RRSIG	A 8 3 10200 20170612005010 20170515005010 42393 nlnetlabs.nl. NhEDrHkuIgHkjWhDRVsGOIJWZpSs+QdduilWFe5d+/ZhOheLJbaTYD5w6+ZZ3yPh1tNud+jlg+GyiOSVapLEO31swDCIarL1UfRjRSpxxDCHGag5Zu+S4hF+KURxO3cJk8jLBELMQyRuMRHoKrw/wsiLGVu1YpAyAPPMcjFBNbk= ;{id = 42393}\n"
/* just an RRSIG rrset with nothing else, 2 rrsigs */
"z5.example.com.	3600	IN	RRSIG	A 8 3 10200 20170612005010 20170515005010 42393 nlnetlabs.nl. NhEDrHkuIgHkjWhDRVsGOIJWZpSs+QdduilWFe5d+/ZhOheLJbaTYD5w6+ZZ3yPh1tNud+jlg+GyiOSVapLEO31swDCIarL1UfRjRSpxxDCHGag5Zu+S4hF+KURxO3cJk8jLBELMQyRuMRHoKrw/wsiLGVu1YpAyAPPMcjFBNbk= ;{id = 42393}\n"
"z5.example.com.	3600	IN	RRSIG	A 8 3 10200 20170612005010 20170515005010 12345 nlnetlabs.nl. NhEDrHkuIgHkjWhDRVsGOIJWZpSs+QdduilWFe5d+/ZhOheLJbaTYD5w6+ZZ3yPh1tNud+jlg+GyiOSVapLEO31swDCIarL1UfRjRSpxxDCHGag5Zu+S4hF+KURxO3cJk8jLBELMQyRuMRHoKrw/wsiLGVu1YpAyAPPMcjFBNbk= ;{id = 12345}\n"
#if 0 /* comparison of file does not work on this part because duplicates */
      /* are removed and the rrsets are reordered */
/* first rrsig, then A record */
"z6.example.com.	3600	IN	RRSIG	A 8 3 10200 20170612005010 20170515005010 42393 nlnetlabs.nl. NhEDrHkuIgHkjWhDRVsGOIJWZpSs+QdduilWFe5d+/ZhOheLJbaTYD5w6+ZZ3yPh1tNud+jlg+GyiOSVapLEO31swDCIarL1UfRjRSpxxDCHGag5Zu+S4hF+KURxO3cJk8jLBELMQyRuMRHoKrw/wsiLGVu1YpAyAPPMcjFBNbk= ;{id = 42393}\n"
"z6.example.com.	3600	IN	A	10.0.0.10\n"
/* first two rrsigs, then A record */
"z7.example.com.	3600	IN	RRSIG	A 8 3 10200 20170612005010 20170515005010 42393 nlnetlabs.nl. NhEDrHkuIgHkjWhDRVsGOIJWZpSs+QdduilWFe5d+/ZhOheLJbaTYD5w6+ZZ3yPh1tNud+jlg+GyiOSVapLEO31swDCIarL1UfRjRSpxxDCHGag5Zu+S4hF+KURxO3cJk8jLBELMQyRuMRHoKrw/wsiLGVu1YpAyAPPMcjFBNbk= ;{id = 42393}\n"
"z7.example.com.	3600	IN	RRSIG	A 8 3 10200 20170612005010 20170515005010 12345 nlnetlabs.nl. NhEDrHkuIgHkjWhDRVsGOIJWZpSs+QdduilWFe5d+/ZhOheLJbaTYD5w6+ZZ3yPh1tNud+jlg+GyiOSVapLEO31swDCIarL1UfRjRSpxxDCHGag5Zu+S4hF+KURxO3cJk8jLBELMQyRuMRHoKrw/wsiLGVu1YpAyAPPMcjFBNbk= ;{id = 12345}\n"
"z7.example.com.	3600	IN	A	10.0.0.10\n"
/* first two rrsigs, then two A records */
"z8.example.com.	3600	IN	RRSIG	A 8 3 10200 20170612005010 20170515005010 42393 nlnetlabs.nl. NhEDrHkuIgHkjWhDRVsGOIJWZpSs+QdduilWFe5d+/ZhOheLJbaTYD5w6+ZZ3yPh1tNud+jlg+GyiOSVapLEO31swDCIarL1UfRjRSpxxDCHGag5Zu+S4hF+KURxO3cJk8jLBELMQyRuMRHoKrw/wsiLGVu1YpAyAPPMcjFBNbk= ;{id = 42393}\n"
"z8.example.com.	3600	IN	RRSIG	A 8 3 10200 20170612005010 20170515005010 12345 nlnetlabs.nl. NhEDrHkuIgHkjWhDRVsGOIJWZpSs+QdduilWFe5d+/ZhOheLJbaTYD5w6+ZZ3yPh1tNud+jlg+GyiOSVapLEO31swDCIarL1UfRjRSpxxDCHGag5Zu+S4hF+KURxO3cJk8jLBELMQyRuMRHoKrw/wsiLGVu1YpAyAPPMcjFBNbk= ;{id = 12345}\n"
"z8.example.com.	3600	IN	A	10.0.0.10\n"
"z8.example.com.	3600	IN	A	10.0.0.11\n"
/* duplicate RR, duplicate RRsig */
"z9.example.com.	3600	IN	A	10.0.0.10\n"
"z9.example.com.	3600	IN	A	10.0.0.11\n"
"z9.example.com.	3600	IN	A	10.0.0.10\n"
"z9.example.com.	3600	IN	RRSIG	A 8 3 10200 20170612005010 20170515005010 42393 nlnetlabs.nl. NhEDrHkuIgHkjWhDRVsGOIJWZpSs+QdduilWFe5d+/ZhOheLJbaTYD5w6+ZZ3yPh1tNud+jlg+GyiOSVapLEO31swDCIarL1UfRjRSpxxDCHGag5Zu+S4hF+KURxO3cJk8jLBELMQyRuMRHoKrw/wsiLGVu1YpAyAPPMcjFBNbk= ;{id = 42393}\n"
"z9.example.com.	3600	IN	RRSIG	A 8 3 10200 20170612005010 20170515005010 42393 nlnetlabs.nl. NhEDrHkuIgHkjWhDRVsGOIJWZpSs+QdduilWFe5d+/ZhOheLJbaTYD5w6+ZZ3yPh1tNud+jlg+GyiOSVapLEO31swDCIarL1UfRjRSpxxDCHGag5Zu+S4hF+KURxO3cJk8jLBELMQyRuMRHoKrw/wsiLGVu1YpAyAPPMcjFBNbk= ;{id = 42393}\n"
#endif /* if0 for duplicates and reordering */
;

/** queries for example.com: zone, query, flags, answer. end with NULL */
static struct q_ans example_com_queries[] = {
	{"example.com", "www.example.com. A", "",
";flags QR AA rcode NOERROR\n"
";answer section\n"
"www.example.com.	3600	IN	A	10.0.0.2\n"
"www.example.com.	3600	IN	A	10.0.0.3\n"
	},
	{NULL, NULL, NULL, NULL}
};

/** number of tmpfiles */
static int tempno = 0;
/** number of deleted files */
static int delno = 0;

/** cleanup tmp files at exit */
static void
tmpfilecleanup(void)
{
	int i;
	char buf[256];
	for(i=0; i<tempno; i++) {
		snprintf(buf, sizeof(buf), "/tmp/unbound.unittest.%u.%d",
			(unsigned)getpid(), i);
		if(vbmp) printf("cleanup: unlink %s\n", buf);
		unlink(buf);
	}
}

/** create temp file, return (malloced) name string, write contents to it */
static char*
create_tmp_file(const char* s)
{
	char buf[256];
	char *fname;
	FILE *out;
	size_t r;
	snprintf(buf, sizeof(buf), "/tmp/unbound.unittest.%u.%d",
		(unsigned)getpid(), tempno++);
	fname = strdup(buf);
	if(!fname) fatal_exit("out of memory");
	/* if no string, just make the name */
	if(!s) return fname;
	/* if string, write to file */
	out = fopen(fname, "w");
	if(!out) fatal_exit("cannot open %s: %s", fname, strerror(errno));
	r = fwrite(s, 1, strlen(s), out);
	if(r == 0) {
		fatal_exit("write failed: %s", strerror(errno));
	} else if(r < strlen(s)) {
		fatal_exit("write failed: too short (disk full?)");
	}
	fclose(out);
	return fname;
}

/** delete temp file and free name string */
static void
del_tmp_file(char* fname)
{
	unlink(fname);
	free(fname);
	delno++;
	if(delno == tempno) {
		/* deleted all outstanding files, back to start condition */
		tempno = 0;
		delno = 0;
	}
}

/** Add zone from file for testing */
static struct auth_zone*
addzone(struct auth_zones* az, const char* name, char* fname)
{
	struct auth_zone* z;
	size_t nmlen;
	uint8_t* nm = sldns_str2wire_dname(name, &nmlen);
	if(!nm) fatal_exit("out of memory");
	lock_rw_wrlock(&az->lock);
	z = auth_zone_create(az, nm, nmlen, LDNS_RR_CLASS_IN);
	lock_rw_unlock(&az->lock);
	if(!z) fatal_exit("cannot find zone");
	auth_zone_set_zonefile(z, fname);

	if(!auth_zone_read_zonefile(z)) {
		fatal_exit("parse failure for auth zone %s", name);
	}
	lock_rw_unlock(&z->lock);
	free(nm);
	return z;
}

/** check that file is the same as other file */
static void
checkfile(char* f1, char *f2)
{
	char buf1[10240], buf2[10240];
	int line = 0;
	FILE* i1, *i2;
	i1 = fopen(f1, "r");
	if(!i1) fatal_exit("cannot open %s: %s", f1, strerror(errno));
	i2 = fopen(f2, "r");
	if(!i2) fatal_exit("cannot open %s: %s", f2, strerror(errno));

	while(!feof(i1) && !feof(i2)) {
		line++;
		(void)fgets(buf1, (int)sizeof(buf1), i1);
		(void)fgets(buf2, (int)sizeof(buf2), i2);
		if(strcmp(buf1, buf2) != 0) {
			log_info("in files %s and %s:%d", f1, f2, line);
			log_info("'%s'", buf1);
			log_info("'%s'", buf2);
			fatal_exit("files are not eqaul");
		}
	}
	unit_assert(feof(i1) && feof(i2));

	fclose(i1);
	fclose(i2);
}

/** check that a zone (in string) can be read and reproduced */
static void
check_read_exact(const char* name, const char* zone)
{
	struct auth_zones* az;
	struct auth_zone* z;
	char* fname, *outf;
	if(vbmp) printf("check read zone %s\n", name);
	fname = create_tmp_file(zone);

	az = auth_zones_create();
	unit_assert(az);
	z = addzone(az, name, fname);
	unit_assert(z);
	outf = create_tmp_file(NULL);
	if(!auth_zone_write_file(z, outf)) {
		fatal_exit("write file failed for %s", fname);
	}
	checkfile(fname, outf);

	del_tmp_file(fname);
	del_tmp_file(outf);
	auth_zones_delete(az);
}

/** parse q_ans structure for making query */
static void
q_ans_parse(struct q_ans* q, struct regional* region,
	struct query_info** qinfo, int* fallback, uint8_t** dp_nm,
	size_t* dp_nmlen)
{
	int ret;
	uint8_t buf[65535];
	size_t len, dname_len;

	/* parse flags */
	*fallback = 0; /* default fallback value */
	if(strstr(q->flags, "fallback"))
		*fallback = 1;
	
	/* parse zone */
	*dp_nmlen = sizeof(buf);
	if((ret=sldns_str2wire_dname_buf(q->zone, buf, dp_nmlen))!=0)
		fatal_exit("cannot parse query dp zone %s : %s", q->zone,
			sldns_get_errorstr_parse(ret));
	*dp_nm = regional_alloc_init(region, buf, *dp_nmlen);
	if(!dp_nm) fatal_exit("out of memory");

	/* parse query */
	len = sizeof(buf);
	dname_len = 0;
	if((ret=sldns_str2wire_rr_question_buf(q->query, buf, &len, &dname_len,
		*dp_nm, *dp_nmlen, NULL, 0))!=0)
		fatal_exit("cannot parse query %s : %s", q->query,
			sldns_get_errorstr_parse(ret));
	*qinfo = (struct query_info*)regional_alloc_zero(region,
		sizeof(**qinfo));
	if(!*qinfo) fatal_exit("out of memory");
	(*qinfo)->qname = regional_alloc_init(region, buf, dname_len);
	if(!(*qinfo)->qname) fatal_exit("out of memory");
	(*qinfo)->qname_len = dname_len;
	(*qinfo)->qtype = sldns_wirerr_get_type(buf, len, dname_len);
	(*qinfo)->qclass = sldns_wirerr_get_class(buf, len, dname_len);
}

/** print flags to string */
static void
pr_flags(sldns_buffer* buf, uint16_t flags)
{
	char rcode[32];
	sldns_buffer_printf(buf, ";flags");
	if((flags&BIT_QR)!=0) sldns_buffer_printf(buf, " QR");
	if((flags&BIT_AA)!=0) sldns_buffer_printf(buf, " AA");
	if((flags&BIT_TC)!=0) sldns_buffer_printf(buf, " TC");
	if((flags&BIT_RD)!=0) sldns_buffer_printf(buf, " RD");
	if((flags&BIT_CD)!=0) sldns_buffer_printf(buf, " CD");
	if((flags&BIT_RA)!=0) sldns_buffer_printf(buf, " RA");
	if((flags&BIT_AD)!=0) sldns_buffer_printf(buf, " AD");
	if((flags&BIT_Z)!=0) sldns_buffer_printf(buf, " Z");
	sldns_wire2str_rcode_buf((int)(FLAGS_GET_RCODE(flags)),
		rcode, sizeof(rcode));
	sldns_buffer_printf(buf, " rcode %s", rcode);
	sldns_buffer_printf(buf, "\n");
}

/** print RRs to string */
static void
pr_rrs(sldns_buffer* buf, struct reply_info* rep)
{
	char s[65536];
	size_t i, j;
	struct packed_rrset_data* d;
	log_assert(rep->rrset_count == rep->an_numrrsets + rep->ns_numrrsets
		+ rep->ar_numrrsets);
	for(i=0; i<rep->rrset_count; i++) {
		/* section heading */
		if(i == 0 && rep->an_numrrsets != 0)
			sldns_buffer_printf(buf, ";answer section\n");
		else if(i == rep->an_numrrsets && rep->ns_numrrsets != 0)
			sldns_buffer_printf(buf, ";authority section\n");
		else if(i == rep->an_numrrsets+rep->ns_numrrsets &&
			rep->ar_numrrsets != 0)
			sldns_buffer_printf(buf, ";additional section\n");
		/* spool RRset */
		d = (struct packed_rrset_data*)rep->rrsets[i]->entry.data;
		for(j=0; j<d->count+d->rrsig_count; j++) {
			if(!packed_rr_to_string(rep->rrsets[i], j, 0,
				s, sizeof(s))) {
				fatal_exit("could not rr_to_string %d",
					(int)i);
			}
			sldns_buffer_printf(buf, "%s", s);
		}
	}
}

/** create string for message */
static char*
msgtostr(struct dns_msg* msg)
{
	char* str;
	sldns_buffer* buf = sldns_buffer_new(65535);
	if(!buf) fatal_exit("out of memory");
	pr_flags(buf, msg->rep->flags);
	pr_rrs(buf, msg->rep);

	str = strdup((char*)sldns_buffer_begin(buf));
	if(!str) fatal_exit("out of memory");
	sldns_buffer_free(buf);
	return str;
}

/** make q_ans query */
static void
q_ans_query(struct q_ans* q, struct auth_zones* az, struct query_info* qinfo,
	struct regional* region, int expected_fallback, uint8_t* dp_nm,
	size_t dp_nmlen)
{
	int ret, fallback = 0;
	struct dns_msg* msg = NULL;
	char* ans_str;
	int oldv = verbosity;
	/* increase verbosity to printout logic in authzone */
	if(vbmp) verbosity = 4;
	ret = auth_zones_lookup(az, qinfo, region, &msg, &fallback, dp_nm,
		dp_nmlen);
	if(vbmp) verbosity = oldv;

	/* check the answer */
	ans_str = msgtostr(msg);
	/* printout if vbmp */
	if(vbmp) printf("got (ret=%s%s):\n%s",
		(ret?"ok":"fail"), (fallback?" fallback":""), ans_str);
	/* check expected value for ret */
	/* check expected value for fallback */
	(void)expected_fallback;
	/* check answer string */
	(void)q;
	free(ans_str);
}

/** check queries on a loaded zone */
static void
check_az_q_ans(struct auth_zones* az, struct q_ans* queries)
{
	struct q_ans* q;
	struct regional* region = regional_create();
	struct query_info* qinfo;
	int fallback;
	uint8_t* dp_nm;
	size_t dp_nmlen;
	for(q=queries; q->zone; q++) {
		if(vbmp) printf("query %s: %s %s\n", q->zone, q->query,
			q->flags);
		q_ans_parse(q, region, &qinfo, &fallback, &dp_nm, &dp_nmlen);
		q_ans_query(q, az, qinfo, region, fallback, dp_nm, dp_nmlen);
		regional_free_all(region);
	}
	regional_destroy(region);
}

/** check queries for a zone are returned as specified */
static void
check_queries(const char* name, const char* zone, struct q_ans* queries)
{
	struct auth_zones* az;
	struct auth_zone* z;
	char* fname;
	if(vbmp) printf("check queries %s\n", name);
	fname = create_tmp_file(zone);
	az = auth_zones_create();
	if(!az) fatal_exit("out of memory");
	z = addzone(az, name, fname);
	if(!z) fatal_exit("could not read zone for queries test");
	del_tmp_file(fname);

	/* run queries and test them */
	check_az_q_ans(az, queries);

	auth_zones_delete(az);
}

/** Test authzone read from file */
static void
authzone_read_test(void)
{
	if(vbmp) printf("Testing read auth zone\n");
	check_read_exact("example.com", zone_example_com);
}

/** Test authzone query from zone */
static void
authzone_query_test(void)
{
	if(vbmp) printf("Testing query auth zone\n");
	check_queries("example.com", zone_example_com, example_com_queries);
}

/** test authzone code */
void 
authzone_test(void)
{
	unit_show_feature("authzone");
	atexit(tmpfilecleanup);
	authzone_read_test();
	authzone_query_test();
	/*exit(0);*/ /* DEBUG */
}
