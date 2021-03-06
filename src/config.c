/*
 * Copyright (C) 2013 Nikos Mavrogiannopoulos
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <fcntl.h>
#include <ocserv-args.h>
#include <autoopts/options.h>
#include <limits.h>

#include <vpn.h>
#include <tlslib.h>

#define DEFAULT_CFG_FILE "/etc/ocserv.conf"

static const char* pid_file = NULL;
static const char* cfg_file = DEFAULT_CFG_FILE;

#define MAX_ENTRIES 64

#define READ_MULTI_LINE(name, s_name, num, mand) \
	val = optionGetValue(pov, name); \
	if (val != NULL && val->valType == OPARG_TYPE_STRING) { \
		if (s_name == NULL) { \
			num = 0; \
			s_name = malloc(sizeof(char*)*MAX_ENTRIES); \
			do { \
			        if (val && !strcmp(val->pzName, name)==0) \
					continue; \
			        s_name[num] = strdup(val->v.strVal); \
			        num++; \
			        if (num>=MAX_ENTRIES) \
			        break; \
		      } while((val = optionNextValue(pov, val)) != NULL); \
		      s_name[num] = NULL; \
		} \
	} else if (mand != 0) { \
		fprintf(stderr, "Configuration option %s is mandatory.\n", name); \
		exit(1); \
	}

#define READ_BOOLEAN(name, s_name, mand) \
	val = optionGetValue(pov, name); \
	if (val != NULL) { \
		s_name = 1; \
	} else if (mand != 0) { \
		fprintf(stderr, "Configuration option %s is mandatory.\n", name); \
		exit(1); \
	}


#define READ_STRING(name, s_name, mand) \
	val = optionGetValue(pov, name); \
	if (val != NULL && val->valType == OPARG_TYPE_STRING) \
		s_name = strdup(val->v.strVal); \
	else if (mand != 0) { \
		fprintf(stderr, "Configuration option %s is mandatory.\n", name); \
		exit(1); \
	}
	
#define READ_TF(name, s_name, mand, def) \
	{ char* tmp_tf = NULL; \
		READ_STRING(name, tmp_tf, mand); \
		if (tmp_tf == NULL) s_name = def; \
		else { \
			if (strcasecmp(tmp_tf, "true") == 0 || strcasecmp(tmp_tf, "yes") == 0) \
				s_name = 1; \
			else \
				s_name = 0; \
		} \
	}

#define READ_NUMERIC(name, s_name, mand) \
	val = optionGetValue(pov, name); \
	if (val != NULL) { \
		if (val->valType == OPARG_TYPE_NUMERIC) \
			s_name = val->v.longVal; \
		else if (val->valType == OPARG_TYPE_STRING) \
			s_name = atoi(val->v.strVal); \
	} else if (mand != 0) { \
		fprintf(stderr, "Configuration option %s is mandatory.\n", name); \
		exit(1); \
	}

static void parse_cfg_file(const char* file, struct cfg_st *config)
{
tOptionValue const * pov;
const tOptionValue* val;
char** auth = NULL;
unsigned auth_size = 0;
unsigned j;

	pov = configFileLoad(file);
	if (pov == NULL) {
		fprintf(stderr, "Error loading config file %s\n", file);
		exit(1);
	}

	READ_MULTI_LINE("auth", auth, auth_size, 1);
	for (j=0;j<auth_size;j++) {
		if (strcasecmp(auth[j], "pam") == 0) {
			if ((config->auth_types & AUTH_TYPE_USERNAME_PASS) != 0) {
				fprintf(stderr, "You cannot mix multiple username/password authentication methods\n");
				exit(1);
			}
#ifdef HAVE_PAM
			config->auth_types |= AUTH_TYPE_PAM;
#else
			fprintf(stderr, "PAM support is disabled\n");
			exit(1);
#endif
		} else if (strncasecmp(auth[j], "plain[", 6) == 0) {
			char* p;

			if ((config->auth_types & AUTH_TYPE_USERNAME_PASS) != 0) {
				fprintf(stderr, "You cannot mix multiple username/password authentication methods\n");
				exit(1);
			}

			config->plain_passwd = strdup(auth[j]+6);
			p = strchr(config->plain_passwd, ']');
			if (p == NULL) {
				fprintf(stderr, "Format error in %s\n", auth[j]);
				exit(1);
			}
			*p = 0;
			config->auth_types |= AUTH_TYPE_PLAIN;
		} else if (strcasecmp(auth[j], "certificate") == 0) {
			config->auth_types |= AUTH_TYPE_CERTIFICATE;
		} else {
			fprintf(stderr, "Unknown auth method: %s\n", auth[j]);
			exit(1);
		}
		free(auth[j]);
	}
	free(auth);
	
	/* When adding allocated data, remember to modify
	 * reload_cfg_file();
	 */
	READ_STRING("listen-host", config->name, 0);

	READ_NUMERIC("tcp-port", config->port, 1);
	READ_NUMERIC("udp-port", config->udp_port, 0);
	READ_NUMERIC("keepalive", config->keepalive, 0);
	READ_NUMERIC("dpd", config->dpd, 0);
	READ_NUMERIC("rate-limit-ms", config->rate_limit_ms, 0);

	READ_STRING("ocsp-response", config->ocsp_response, 0);
	READ_MULTI_LINE("server-cert", config->cert, config->cert_size, 1);
	READ_MULTI_LINE("server-key", config->key, config->key_size, 1);
	READ_STRING("dh-params", config->dh_params_file, 0);
	READ_STRING("pin-file", config->pin_file, 0);
	READ_STRING("srk-pin-file", config->srk_pin_file, 0);
#ifdef ANYCONNECT_CLIENT_COMPAT
	READ_STRING("user-profile", config->xml_config_file, 0);
#endif

	READ_STRING("ca-cert", config->ca, 0);
	READ_STRING("default-domain", config->default_domain, 0);
	READ_STRING("crl", config->crl, 0);
	READ_STRING("cert-user-oid", config->cert_user_oid, 0);
	READ_STRING("cert-group-oid", config->cert_group_oid, 0);

	READ_STRING("connect-script", config->connect_script, 0);
	READ_STRING("disconnect-script", config->disconnect_script, 0);
	
	if (pid_file == NULL)
		READ_STRING("pid-file", pid_file, 0);

	READ_STRING("socket-file", config->socket_file_prefix, 1);

	READ_STRING("banner", config->banner, 0);
	READ_TF("always-require-cert", config->force_cert_auth, 0, 1);
	READ_TF("use-utmp", config->use_utmp, 0, 1);
	READ_TF("try-mtu-discovery", config->try_mtu, 0, 0);
	READ_TF("ping-leases", config->ping_leases, 0, 0);

	READ_STRING("tls-priorities", config->priorities, 0);
	READ_STRING("chroot-dir", config->chroot_dir, 0);

	READ_NUMERIC("cookie-validity", config->cookie_validity, 1);
	READ_NUMERIC("auth-timeout", config->auth_timeout, 0);
	READ_NUMERIC("max-clients", config->max_clients, 0);
	READ_NUMERIC("min-reauth-time", config->min_reauth_time, 0);
	READ_NUMERIC("max-same-clients", config->max_same_clients, 0);

	val = optionGetValue(pov, "run-as-user"); \
	if (val != NULL && val->valType == OPARG_TYPE_STRING) {
		const struct passwd* pwd = getpwnam(val->v.strVal);
		if (pwd == NULL) {
			fprintf(stderr, "Unknown user: %s\n", val->v.strVal);
			exit(1);
		}
		config->uid = pwd->pw_uid;
	}

	val = optionGetValue(pov, "run-as-group"); \
	if (val != NULL && val->valType == OPARG_TYPE_STRING) {
		const struct group *grp = getgrnam(val->v.strVal);
		if (grp == NULL) {
			fprintf(stderr, "Unknown group: %s\n", val->v.strVal);
			exit(1);
		}
		config->gid = grp->gr_gid;
	}

	READ_STRING("device", config->network.name, 1);

	READ_STRING("ipv4-network", config->network.ipv4, 0);
	READ_STRING("ipv4-netmask", config->network.ipv4_netmask, 0);
	READ_STRING("ipv4-dns", config->network.ipv4_dns, 0);

	READ_STRING("ipv6-network", config->network.ipv6, 0);
	READ_STRING("ipv6-netmask", config->network.ipv6_netmask, 0);
	READ_STRING("ipv6-dns", config->network.ipv6_dns, 0);

	READ_STRING("ipv4-nbns", config->network.ipv4_nbns, 0);
	READ_STRING("ipv6-nbns", config->network.ipv6_nbns, 0);

	READ_MULTI_LINE("route", config->network.routes, config->network.routes_size, 0);
	
	optionUnloadNested(pov);
}


/* sanity checks on config */
static void check_cfg( struct cfg_st *config)
{
	if (config->network.ipv4 == NULL && config->network.ipv6 == NULL) {
		fprintf(stderr, "No ipv4-network or ipv6-network options set.\n");
		exit(1);
	}

	if (config->network.ipv4 != NULL && config->network.ipv4_netmask == NULL) {
		fprintf(stderr, "No mask found for IPv4 network.\n");
		exit(1);
	}

	if (config->network.ipv6 != NULL && config->network.ipv6_netmask == NULL) {
		fprintf(stderr, "No mask found for IPv6 network.\n");
		exit(1);
	}
	
	if (config->banner && strlen(config->banner) > MAX_BANNER_SIZE) {
		fprintf(stderr, "Banner size is too long\n");
		exit(1);
	}

	if (config->cert_size != config->key_size) {
		fprintf(stderr, "The specified number of keys doesn't match the certificates\n");
		exit(1);
	}

	if (config->auth_types & AUTH_TYPE_CERTIFICATE) {
		if (config->force_cert_auth)
			config->cert_req = GNUTLS_CERT_REQUIRE;
		else
			config->cert_req = GNUTLS_CERT_REQUEST;
	}
	
	if (config->plain_passwd != NULL) {
		if (access(config->plain_passwd, R_OK) != 0) {
			fprintf(stderr, "cannot access password file '%s'\n", config->plain_passwd);
			exit(1);
		}
	}

#ifdef ANYCONNECT_CLIENT_COMPAT
	if (config->cert) {
		config->cert_hash = calc_sha1_hash(config->cert[0], 1);
	}

	if (config->xml_config_file) {
		config->xml_config_hash = calc_sha1_hash(config->xml_config_file, 0);
		if (config->xml_config_hash == NULL && config->chroot_dir != NULL) {
			char path[_POSIX_PATH_MAX];
			
			snprintf(path, sizeof(path), "%s/%s", config->chroot_dir, config->xml_config_file);
			config->xml_config_hash = calc_sha1_hash(path, 0);
			
			if (config->xml_config_hash == NULL) {
				fprintf(stderr, "Cannot open file '%s'\n", path);
				exit(1);
			}
		}
		if (config->xml_config_hash == NULL) {
			fprintf(stderr, "Cannot open file '%s'\n", config->xml_config_file);
			exit(1);
		}
	}
#endif
	
	if (config->keepalive == 0)
		config->keepalive = 3600;

	if (config->dpd == 0)
		config->keepalive = 60;

	if (config->udp_port == 0)
		config->udp_port = config->port;
	
	if (config->priorities == NULL)
		config->priorities = "NORMAL:%SERVER_PRECEDENCE:%COMPAT";
}

int cmd_parser (int argc, char **argv, struct cfg_st* config)
{

	memset(config, 0, sizeof(*config));

	optionProcess( &ocservOptions, argc, argv);
  
	if (HAVE_OPT(FOREGROUND))
		config->foreground = 1;

	if (HAVE_OPT(TLS_DEBUG))
		config->tls_debug = 1;

	if (HAVE_OPT(DEBUG))
		config->debug = 1;
	
	if (HAVE_OPT(CONFIG)) {
		cfg_file = OPT_ARG(CONFIG);
	} else if (access(cfg_file, R_OK) != 0) {
		fprintf(stderr, "%s -c [config]\nUse %s --help for more information.\n", argv[0], argv[0]);
		exit(1);
	}
	
	parse_cfg_file(cfg_file, config);
	
	check_cfg(config);
	
	return 0;

}

#define DEL(x) free(x);x=NULL
void reload_cfg_file(struct cfg_st* config)
{
unsigned i;

#ifdef ANYCONNECT_CLIENT_COMPAT
	DEL(config->xml_config_file);
	DEL(config->xml_config_hash);
	DEL(config->cert_hash);
#endif
	DEL(config->socket_file_prefix);
	DEL(config->default_domain);
	DEL(config->plain_passwd);
	DEL(config->ocsp_response);
	DEL(config->banner);
	DEL(config->dh_params_file);
	DEL(config->name);
	DEL(config->pin_file);
	DEL(config->srk_pin_file);
	DEL(config->ca);
	DEL(config->crl);
	DEL(config->cert_user_oid);
	DEL(config->cert_group_oid);
	DEL(config->priorities);
	DEL(config->chroot_dir);
	DEL(config->connect_script);
	DEL(config->disconnect_script);

	DEL(config->network.name);
	DEL(config->network.ipv4);
	DEL(config->network.ipv4_netmask);
	DEL(config->network.ipv4_dns);
	DEL(config->network.ipv6);
	DEL(config->network.ipv6_netmask);
	DEL(config->network.ipv6_dns);
	for (i=0;i<config->network.routes_size;i++)
		DEL(config->network.routes[i]);
	for (i=0;i<config->key_size;i++)
		DEL(config->key[i]);
	DEL(config->key);
	for (i=0;i<config->cert_size;i++)
		DEL(config->cert[i]);
	DEL(config->cert);
	DEL(config->network.routes);
	memset(config, 0, sizeof(*config));

	parse_cfg_file(cfg_file, config);

	check_cfg(config);
	
	return;
}

void write_pid_file(void)
{
FILE* fp;

	if (pid_file==NULL)
		return;

	fp = fopen(pid_file, "w");
	if (fp == NULL) {
		fprintf(stderr, "Cannot open pid file '%s'\n", pid_file);
		exit(1);
	}
	
	fprintf(fp, "%u", (unsigned)getpid());
	fclose(fp);
}

void remove_pid_file(void)
{
	if (pid_file==NULL)
		return;

	remove(pid_file);
}
