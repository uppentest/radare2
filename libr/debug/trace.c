/* radare - LGPL - Copyright 2008-2010 pancake<nopcode.org> */

#include <r_debug.h>

R_API RDebugTrace *r_debug_trace_new () {
	RDebugTrace *t = R_NEW (RDebugTrace);
	t->tag = 1; // UT32_MAX;
	t->enabled = R_FALSE;
	t->traces = r_list_new ();
	t->traces->free = free;
	return t;
}

R_API void r_debug_trace_free (RDebug *dbg) {
	if (dbg->trace == NULL)
		return;
	r_list_destroy (dbg->trace->traces);
	free (dbg->trace);
	dbg->trace = NULL;
}

// TODO: added overlap/mask support here... wtf?
// TODO: think about tagged traces
R_API int r_debug_trace_tag (RDebug *dbg, int tag) {
	//if (tag>0 && tag<31) core->dbg->trace->tag = 1<<(sz-1);
	return (dbg->trace->tag = (tag>0)? tag: UT32_MAX);
}

R_API int r_debug_trace_pc (RDebug *dbg) {
	ut8 buf[32];
	RRegisterItem *ri;
	r_debug_reg_sync (dbg, R_REG_TYPE_GPR, R_FALSE);
	if ((ri = r_reg_get (dbg->reg, dbg->reg->name[R_REG_NAME_PC], -1))) {
		ut64 addr = r_reg_get_value (dbg->reg, ri);
		if (dbg->iob.read_at (dbg->iob.io, addr, buf, sizeof (buf))>0) {
			RAnalOp aop;
			if (r_anal_aop (dbg->anal, &aop, addr, buf, sizeof (buf))>0) {
				r_debug_trace_add (dbg, addr, aop.length);
				return R_TRUE;
			} else eprintf ("trace_pc: cannot get opcode size at 0x%"PFMT64x"\n", addr);
		} else eprintf ("trace_pc: cannot read memory at 0x%"PFMT64x"\n", addr);
	} else eprintf ("trace_pc: cannot get program counter\n");
	return R_FALSE;
}

R_API RDebugTracepoint *r_debug_trace_get (RDebug *dbg, ut64 addr) {
	/* TODO: handle opcode size .. warn when jumping in the middle of instructions */
	int tag = dbg->trace->tag;
	RListIter *iter = r_list_iterator (dbg->trace->traces);
	while (r_list_iter_next (iter)) {
		RDebugTracepoint *trace = (RDebugTracepoint *)r_list_iter_get (iter);
		if (tag != 0 && !(dbg->trace->tag & (1<<tag)))
			continue;
		if (trace->addr == addr)
			return trace;
	}
	return NULL;
}

R_API void r_debug_trace_list (RDebug *dbg, int mode) {
	int tag = dbg->trace->tag;
	RListIter *iter = r_list_iterator (dbg->trace->traces);
	while (r_list_iter_next (iter)) {
		RDebugTracepoint *trace = r_list_iter_get (iter);
		if (!trace->tag || (tag & trace->tag)) {
			if (mode == 1)
				dbg->printf ("at+ 0x%"PFMT64x" %d\n", trace->addr, trace->times);
			else dbg->printf ("0x%08"PFMT64x" size=%d count=%d times=%d tag=%d\n",
				trace->addr, trace->size, trace->count, trace->times, trace->tag);
		}
	}
}

/* sort insert, or separated sort function ? */
/* TODO: detect if inner opcode */
R_API RDebugTracepoint *r_debug_trace_add (RDebug *dbg, ut64 addr, int size) {
	int tag = dbg->trace->tag;
	RDebugTracepoint *tp = r_debug_trace_get (dbg, addr);
	if (!tp) {
		tp = R_NEW (RDebugTracepoint);
		memset (tp, 0, sizeof (RDebugTracepoint));
		tp->stamp = r_sys_now ();
		tp->addr = addr;
		tp->tags = tag;
		tp->size = size;
		tp->count = dbg->trace->count++;
		tp->times = 0;
		r_list_append (dbg->trace->traces, tp);
	} else tp->times++;
	return tp;
}
