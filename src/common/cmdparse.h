// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*- 
// vim: ts=8 sw=2 smarttab
#ifndef CEPH_COMMON_CMDPARSE_H
#define CEPH_COMMON_CMDPARSE_H

#include <string>
#include <sstream>
#include <map>
#include <boost/variant.hpp>
#include <vector>
#include <stdexcept>
#include "common/Formatter.h"
#include "common/BackTrace.h"
#include "common/ceph_context.h"

/* this is handy; can't believe it's not standard */
#define ARRAY_SIZE(a)	(sizeof(a) / sizeof(*a))

typedef boost::variant<std::string, bool, int64_t, double, std::vector<std::string> > cmd_vartype;
typedef std::map<std::string, cmd_vartype> cmdmap_t;

void dump_cmd_to_json(ceph::JSONFormatter *f, const std::string& cmd);
void dump_cmd_and_help_to_json(ceph::JSONFormatter *f,
			       const std::string& secname,
			       const std::string& cmd,
			       const std::string& helptext);
bool cmdmap_from_json(std::vector<std::string> cmd, cmdmap_t *mapp,
		      std::stringstream &ss);

bool cmd_getval(CephContext *cct, cmdmap_t& cmdmap, std::string k,
		std::string& val);
bool cmd_getval(CephContext *cct, cmdmap_t& cmdmap, std::string k, bool& val);
bool cmd_getval(CephContext *cct, cmdmap_t& cmdmap, std::string k,
		int64_t& val);
bool cmd_getval(CephContext *cct, cmdmap_t & cmdmap, std::string k,
		double& val);
bool cmd_getval(CephContext *cct, cmdmap_t& cmdmap, std::string k,
		std::vector<std::string>& val);

void cmd_getval(CephContext *cct, cmdmap_t& cmdmap, std::string k,
		std::string& val, std::string defval);
void cmd_getval(CephContext *cct, cmdmap_t& cmdmap, std::string k,
		bool& val, bool defval);
void cmd_getval(CephContext *cct, cmdmap_t& cmdmap, std::string k,
		int64_t& val, int64_t defval);
void cmd_getval(CephContext *cct, cmdmap_t& cmdmap, std::string k,
		double& val, double defval);
void cmd_getval(CephContext *cct, cmdmap_t& cmdmap, std::string k,
		std::vector<std::string>& val, std::vector<std::string> defval);

void cmd_putval(CephContext *cct, cmdmap_t& cmdmap, std::string k,
		std::string val);
void cmd_putval(CephContext *cct, cmdmap_t& cmdmap, std::string k, bool val);
void cmd_putval(CephContext *cct, cmdmap_t& cmdmap, std::string k, int64_t val);
void cmd_putval(CephContext *cct, cmdmap_t& cmdmap, std::string k, double val);
void cmd_putval(CephContext *cct, cmdmap_t& cmdmap, std::string k,
		std::vector<std::string> val);

#endif
