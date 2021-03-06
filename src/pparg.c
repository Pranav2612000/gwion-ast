#include "gwion_util.h"
#include "gwion_ast.h"

static const char usage[] =
"usage: gwion-lint <options>\n"
"  -D    <macro>   : add a macro\n"
"  -U     <xid>    : remove a macro\n"
"  -I     <path>   : add to include path\n"
"  -h     <path>   : this help\n";

ANN static inline m_str get_arg(Vector v, m_uint *i) {
  const m_str arg = (m_str)vector_at(v, *i);
  const char key = arg[1];
  const m_str str = arg[2] == '\0' ? (m_str)vector_at(v, ++(*i)) : arg + 2;
  if(!str)
    gw_err(_("option '-%c' needs arguments\n"), key);
  return str;
}

ANN static Macro pparg_def(struct PPArg_ *ppa, const m_str str) {
  const ssize_t sz = strlen(str);
  char base[sz + 1];
  m_str def = strchr(str, '(') ?: strchr(str, '=');
  const ssize_t idx = def ? (def - str) : sz;
  strncpy(base, str, idx);
  base[idx] = '\0';
  Macro m = macro_add(&ppa->hash, base);
  if(m)
    return m;
  gw_err(MACRO_DEFINED);
  return NULL;
}

ANN static MacroArg pparg_arg(struct PPArg_ *ppa, m_str src) {
  const size_t sz = strlen(src);
  char buf[sz + 1];
  for(m_uint i = 0; i < sz; ++i) {
    const char c = src[i];
    if(c == ')') {
      buf[i] = '\0';
      return new_macroarg(ppa->hash.p, buf);
    }
    if(c == ',') {
      buf[i] = '\0';
      MacroArg arg = new_macroarg(ppa->hash.p, buf);
      arg->next = pparg_arg(ppa, src + i + 1);
      return arg;
    }
    buf[i] = c;
  }
  return NULL;
}

ANN static GwText* pparg_body(struct PPArg_ *ppa, const m_str str) {
  GwText* text = mp_calloc(ppa->hash.p, GwText);
  text->mp = ppa->hash.p;
  text_add(text, str);
  return text;
}

ANN2(1) static m_bool pparg_add(struct PPArg_ *ppa, const m_str str) {
  DECL_OB(const Macro, m, = pparg_def(ppa, str))
  const m_str arg = strchr(str, '(');
  if(arg)
    m->base = pparg_arg(ppa, arg + 1);
  const m_str body = strchr(str, '=');
  if(body)
    m->text = pparg_body(ppa, body + 1);
  m->file = "command line";
  return GW_OK;
}

ANN static inline m_bool pparg_err(void) {
  gw_err(_("invalid arguments"));
  return GW_ERROR;
}

ANN static m_bool _pparg_run(struct PPArg_ *ppa, const Vector v) {
  for(m_uint i = 0; i < vector_size(v); ++i) {
    const m_str base = (m_str)vector_at(v, i);
    if(base[0] == '-') {
      m_str arg;
      CHECK_OB((arg = get_arg(v, &i)))
      switch(base[1]) {
        case 'D': // define
          if(!ppa->hash.table)
            hini(&ppa->hash, 127);
          pparg_add(ppa, arg);
          break;
        case 'U': // undef
          CHECK_BB(macro_rem(&ppa->hash, arg))
          break;
        case 'I': // include
          vector_add(&ppa->path, (vtype)arg);
          break;
        case 'h': // help
          puts(usage);
          break;
        default:
          pparg_err();
      }
    } else
      pparg_err();
  }
  return GW_OK;
}

ANN2(1) m_bool pparg_run(struct PPArg_ *ppa, const Vector v) {
  if(!v)
    return GW_OK;
  return _pparg_run(ppa, v);
}

ANN void pparg_ini(MemPool mp, struct PPArg_* a) {
  a->hash.p = mp;
  vector_init(&a->path);
}

ANN void pparg_end(struct PPArg_* a) {
  vector_release(&a->path);
  if(a->hash.table) {
    macro_del(&a->hash);
    hend(&a->hash);
  }
}
