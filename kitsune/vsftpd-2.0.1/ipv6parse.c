/*
 * Part of Very Secure FTPd
 * Licence: GPL v2
 * Author: Chris Evans
 * ipv6parse.c
 *
 * A routine to parse ipv6 addresses. I'm paranoid and don't want to use
 * inet_pton.
 */

#include "ipv6parse.h"
#include "sysutil.h"
#include "str.h"

static int ipv6_parse_main(struct mystr* p_out_str,
                           const struct mystr* p_in_str);
static int ipv6_parse_hex(struct mystr* p_out_str,
                          const struct mystr* p_in_str);
static int ipv4_parse_dotquad(struct mystr* p_out_str,
                              const struct mystr* p_in_str);

const unsigned char*
vsf_sysutil_parse_ipv6(const struct mystr* p_str)
{
  static struct mystr s_ret;
  static struct mystr s_rhs_ret;
  static struct mystr s_lhs_str;
  static struct mystr s_rhs_str;
  unsigned int lhs_len;
  unsigned int rhs_len;
  str_empty(&s_ret);
  str_empty(&s_rhs_ret);
  str_copy(&s_lhs_str, p_str);
  str_split_text(&s_lhs_str, &s_rhs_str, "::");
  if (!ipv6_parse_main(&s_ret, &s_lhs_str))
  {
    return 0;
  }
  if (!ipv6_parse_main(&s_rhs_ret, &s_rhs_str))
  {
    return 0;
  }
  lhs_len = str_getlen(&s_ret);
  rhs_len = str_getlen(&s_rhs_ret);
  if (lhs_len + rhs_len > 16)
  {
    return 0;
  }
  if (rhs_len > 0)
  {
    unsigned int add_nulls = 16 - (lhs_len + rhs_len);
    while (add_nulls--)
    {
      str_append_char(&s_ret, '\0');
    }
    str_append_str(&s_ret, &s_rhs_ret);
  }
  return str_getbuf(&s_ret);
}

static int
ipv6_parse_main(struct mystr* p_out_str, const struct mystr* p_in_str)
{
  static struct mystr s_lhs_str;
  static struct mystr s_rhs_str;
  struct str_locate_result loc_ret;
  str_copy(&s_lhs_str, p_in_str);
  while (!str_isempty(&s_lhs_str))
  {
    str_split_char(&s_lhs_str, &s_rhs_str, ':');
    if (str_isempty(&s_lhs_str))
    {
      return 0;
    }
    loc_ret = str_locate_char(&s_lhs_str, '.');
    if (loc_ret.found)
    {
      if (!ipv4_parse_dotquad(p_out_str, &s_lhs_str))
      {
        return 0;
      }
    }
    else if (!ipv6_parse_hex(p_out_str, &s_lhs_str))
    {
      return 0;
    }
    str_copy(&s_lhs_str, &s_rhs_str);
  }
  return 1;
}

static int
ipv6_parse_hex(struct mystr* p_out_str, const struct mystr* p_in_str)
{
  unsigned int len = str_getlen(p_in_str);
  unsigned int i;
  unsigned int val = 0;
  for (i=0; i<len; ++i)
  {
    int ch = vsf_sysutil_toupper(str_get_char_at(p_in_str, i));
    if (ch >= '0' && ch <= '9')
    {
      ch -= '0';
    }
    else if (ch >= 'A' && ch <= 'F')
    {
      ch -= 'A';
      ch += 10;
    }
    else
    {
      return 0;
    }
    val <<= 4;
    val |= ch;
    if (val > 0xFFFF)
    {
      return 0;
    }
  }
  str_append_char(p_out_str, (val >> 8));
  str_append_char(p_out_str, (val & 0xFF));
  return 1;
}

static int
ipv4_parse_dotquad(struct mystr* p_out_str, const struct mystr* p_in_str)
{
  unsigned int len = str_getlen(p_in_str);
  unsigned int i;
  unsigned int val = 0;
  unsigned int final_val = 0;
  int seen_char = 0;
  int dots = 0;
  for (i=0; i<len; ++i)
  {
    int ch = str_get_char_at(p_in_str, i);
    if (ch == '.')
    {
      if (!seen_char || dots == 3)
      {
        return 0;
      }
      seen_char = 0;
      dots++;
      final_val <<= 8;
      final_val |= val;
      val = 0;
    }
    else if (ch >= '0' && ch <= '9')
    {
      ch -= '0';
      val *= 10;
      val += ch;
      if (val > 255)
      {
        return 0;
      }
      seen_char = 1;
    }
    else
    {
      return 0;
    }
  }
  if (dots != 3 || !seen_char)
  {
    return 0;
  }
  final_val <<= 8;
  final_val |= val;
  str_append_char(p_out_str, (final_val >> 24));
  str_append_char(p_out_str, ((final_val >> 16) & 0xFF));
  str_append_char(p_out_str, ((final_val >> 8) & 0xFF));
  str_append_char(p_out_str, (final_val & 0xFF));
  return 1;
}

