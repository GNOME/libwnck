#include <libwnck/libwnck.h>


/* VOID:FLAGS,FLAGS (wnck-marshal.list:25) */
void
_wnck_marshal_VOID__FLAGS_FLAGS (GClosure     *closure,
                                 GValue       *return_value,
                                 guint         n_param_values,
                                 const GValue *param_values,
                                 gpointer      invocation_hint,
                                 gpointer      marshal_data)
{
  typedef void (*GMarshalFunc_VOID__FLAGS_FLAGS) (gpointer     data1,
                                                  guint        arg_1,
                                                  guint        arg_2,
                                                  gpointer     data2);
  register GMarshalFunc_VOID__FLAGS_FLAGS callback;
  register GCClosure *cc = (GCClosure*) closure;
  register gpointer data1, data2;

  g_return_if_fail (n_param_values == 3);

  if (G_CCLOSURE_SWAP_DATA (closure))
    {
      data1 = closure->data;
      data2 = g_value_peek_pointer (param_values + 0);
    }
  else
    {
      data1 = g_value_peek_pointer (param_values + 0);
      data2 = closure->data;
    }
  callback = (GMarshalFunc_VOID__FLAGS_FLAGS) (marshal_data ? marshal_data : cc->callback);

  callback (data1,
            g_value_get_flags (param_values + 1),
            g_value_get_flags (param_values + 2),
            data2);
}

