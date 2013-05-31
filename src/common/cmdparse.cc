// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*- 
// vim: ts=8 sw=2 smarttab
/*
 * Ceph - scalable distributed file system
 *
 * Copyright (C) 2013 Inktank Storage, Inc.
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License version 2, as published by the Free Software
 * Foundation.  See file COPYING.
 *
 */

#include "common/cmdparse.h"
#include "include/str_list.h"
#include "json_spirit/json_spirit.h"
#include "common/debug.h"

using namespace std;

/**
 * Read a command description list out of cmd, and dump it to f.
 * A signature description is a set of space-separated words;
 * see MonCommands.h for more info.
 */

void
dump_cmd_to_json(JSONFormatter *f, const string& cmd)
{
  // put whole command signature in an already-opened container
  // elements are: "name", meaning "the typeless name that means a literal"
  // an object {} with key:value pairs representing an argument

  int argnum = 0;
  stringstream ss(cmd);
  string word;

  while (getline(ss, word, ' ')) {
    argnum++;
    // if no , or =, must be a plain word to put out
    if (word.find_first_of(",=") == string::npos) {
      f->dump_string("arg", word);
      continue;
    }
    // Snarf up all the key=val,key=val pairs, put 'em in a dict.
    // no '=val' implies '=True'.
    stringstream argdesc(word);
    string keyval, name;
    map<string, string>desckv;
    // accumulate descriptor keywords in desckv
    size_t pos;

    while (getline(argdesc, keyval, ',')) {
      // key=value; key by itself implies value is bool true
      // name="name" means arg dict will be titled 'name'
      pos = keyval.find('=');
      string key, val;
      if (pos != string::npos) {
	key = keyval.substr(0, pos);
	val = keyval.substr(pos+1);
      } else {
        key = keyval;
        val = true;
      }
      desckv.insert(pair<string, string> (key, val));
    }
    // name the individual desc object based on the name key
    f->open_object_section(desckv["name"].c_str());
    // dump all the keys including name into the array
    for (map<string, string>::iterator it = desckv.begin();
	 it != desckv.end(); it++) {
      f->dump_string(it->first.c_str(), it->second);
    }
    f->close_section(); // attribute object for individual desc
  }
}

void
dump_cmd_and_help_to_json(JSONFormatter *jf,
			  const string& secname,
			  const string& cmdsig,
			  const string& helptext)
{
      jf->open_object_section(secname.c_str());
      jf->open_array_section("sig");
      dump_cmd_to_json(jf, cmdsig);
      jf->close_section(); // sig array
      jf->dump_string("help", helptext.c_str());
      jf->close_section(); // cmd
}

/** Parse JSON in vector cmd into a map from field to map of values
 * (use mValue/mObject)
 * 'cmd' should not disappear over lifetime of map
 * 'mapp' points to the caller's map
 * 'ss' captures any errors during JSON parsing; if function returns
 * false, ss is valid */

bool
cmdmap_from_json(vector<string> cmd, map<string, cmd_vartype> *mapp, stringstream &ss)
{
  json_spirit::mValue v;

  string fullcmd;
  // First, join all cmd strings
  for (vector<string>::iterator it = cmd.begin();
       it != cmd.end(); it++)
    fullcmd += *it;

  try {
    if (!json_spirit::read(fullcmd, v))
      throw runtime_error("unparseable JSON " + fullcmd);
    if (v.type() != json_spirit::obj_type)
      throw(runtime_error("not JSON object " + fullcmd));

    // allocate new mObject (map) to return
    // make sure all contents are simple types (not arrays or objects)
    json_spirit::mObject o = v.get_obj();
    for (map<string, json_spirit::mValue>::iterator it = o.begin();
	 it != o.end(); it++) {

      // ok, marshal it into our string->cmd_vartype map, or throw an
      // exception if it's not a simple datatype.  This is kind of
      // annoying, since json_spirit has a boost::variant inside it
      // already, but it's not public.  Oh well.

      switch (it->second.type()) {

      case json_spirit::obj_type:
      default:
	throw(runtime_error("JSON array/object not allowed " + fullcmd));
        break;

      case json_spirit::array_type:
	{
	  // array is a vector of values.  Unpack it to a vector
	  // of strings, the only type we handle.
	  vector<json_spirit::mValue> spvals = it->second.get_array();
	  vector<string> *outvp = new vector<string>;
	  for (vector<json_spirit::mValue>::iterator sv = spvals.begin();
	       sv != spvals.end(); sv++) {
	    if (sv->type() != json_spirit::str_type)
	      throw(runtime_error("Can't handle arrays of non-strings"));
	    outvp->push_back(sv->get_str());
	  }
	  (*mapp)[it->first] = *outvp;
	}
	break;
      case json_spirit::str_type:
	(*mapp)[it->first] = it->second.get_str();
	break;

      case json_spirit::bool_type:
	(*mapp)[it->first] = it->second.get_bool();
	break;

      case json_spirit::int_type:
	(*mapp)[it->first] = it->second.get_int64();
	break;

      case json_spirit::real_type:
	(*mapp)[it->first] = it->second.get_real();
	break;
      }
    }
    return true;
  } catch (runtime_error e) {
    ss << e.what();
    return false;
  }
}

class stringify_visitor : public boost::static_visitor<string>
{
  public:
    template <typename T>
    string operator()(T &operand) const
      {
	ostringstream oss;
	oss << operand;
	return *(new string(oss.str()));
      }
};

string 
cmd_vartype_stringify(const cmd_vartype &v)
{
  return boost::apply_visitor(stringify_visitor(), v);
}


void
handle_bad_get(CephContext *cct, string k, string typestr)
{
  ostringstream errstr;
  errstr << "bad boost::get: key " << k << " is not type " << typestr;
  lderr(cct) << errstr.str() << dendl;

  BackTrace bt(1);
  ostringstream oss;
  bt.print(oss);
  lderr(cct) << oss << dendl;
}

/*
 * Sadly, g++ wants to screw up if I do the obvious templating on these
 * three cmd_XXXval functions.  Expand them by hand instead: bool cmd_getval(),
 * void cmd_getval() with default value, and cmd_putval().
 */

bool
cmd_getval(CephContext *cct, map<string, cmd_vartype>& cmdmap,
	   string k, string& val)
{
  if (cmdmap.count(k)) {
    try {
      val = boost::get<std::string>(cmdmap[k]);
      return true;
    } catch (boost::bad_get) {
      handle_bad_get(cct, k, "string");
    }
  }
  return false;
}

bool
cmd_getval(CephContext *cct, map<string, cmd_vartype>& cmdmap,
	   string k, bool& val)
{
  if (cmdmap.count(k)) {
    try {
      val = boost::get<bool>(cmdmap[k]);
      return true;
    } catch (boost::bad_get) {
      handle_bad_get(cct, k, "bool");
    }
  }
  return false;
}

bool
cmd_getval(CephContext *cct, map<string, cmd_vartype>& cmdmap,
	   string k, int64_t& val)
{
  if (cmdmap.count(k)) {
    try {
      val = boost::get<int64_t>(cmdmap[k]);
      return true;
    } catch (boost::bad_get) {
      handle_bad_get(cct, k, "int64_t");
    }
  }
  return false;
}

bool
cmd_getval(CephContext *cct, map<string, cmd_vartype>& cmdmap,
	   string k, double& val)
{
  if (cmdmap.count(k)) {
    try {
      val = boost::get<double>(cmdmap[k]);
      return true;
    } catch (boost::bad_get) {
      handle_bad_get(cct, k, "double");
    }
  }
  return false;
}

bool
cmd_getval(CephContext *cct, map<string, cmd_vartype>& cmdmap,
	   string k, vector<string>& val)
{
  if (cmdmap.count(k)) {
    try {
      val = boost::get<std::vector<std::string> >(cmdmap[k]);
      return true;
    } catch (boost::bad_get) {
      handle_bad_get(cct, k, "vector<string>");
    }
  }
  return false;
}

void
cmd_getval(CephContext *cct, map<string, cmd_vartype>& cmdmap,
	   string k, string& val, string defval)
{
  if (!cmd_getval(cct, cmdmap, k, val))
    val = defval;
}

void
cmd_getval(CephContext *cct, map<string, cmd_vartype>& cmdmap,
	   string k, bool& val, bool defval)
{
  if (!cmd_getval(cct, cmdmap, k, val))
    val = defval;
}

void
cmd_getval(CephContext *cct, map<string, cmd_vartype>& cmdmap,
	   string k, int64_t& val, int64_t defval)
{
  if (!cmd_getval(cct, cmdmap, k, val))
    val = defval;
}

void
cmd_getval(CephContext *cct, map<string, cmd_vartype>& cmdmap,
	   string k, double& val, double defval)
{
  if (!cmd_getval(cct, cmdmap, k, val))
    val = defval;
}

void
cmd_getval(CephContext *cct, map<string, cmd_vartype>& cmdmap,
	   string k, vector<string>& val, vector<string> defval)
{
  if (!cmd_getval(cct, cmdmap, k, val))
    val = defval;
}

void
cmd_putval(CephContext *cct, map<string, cmd_vartype>& cmdmap,
	   string k, string val)
{
  cmdmap[k] = val;
}

void
cmd_putval(CephContext *cct, map<string, cmd_vartype>& cmdmap,
	   string k, bool val)
{
  cmdmap[k] = val;
}

void
cmd_putval(CephContext *cct, map<string, cmd_vartype>& cmdmap,
	   string k, int64_t val)
{
  cmdmap[k] = val;
}

void
cmd_putval(CephContext *cct, map<string, cmd_vartype>& cmdmap,
	   string k, double val)
{
  cmdmap[k] = val;
}

void
cmd_putval(CephContext *cct, map<string, cmd_vartype>& cmdmap,
	   string k, vector<string> val)
{
  cmdmap[k] = val;
}
