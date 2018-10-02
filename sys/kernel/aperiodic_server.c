
void dispatch_apr(void *arg) {
	int32_t rc;

#if KERNEL_LOG >= 1
	dprintf("dispatch %d ", (uint32_t)_read_us());
#endif
	_timer_reset();
	if (krnl_schedule == 0) return;
	krnl_task = &krnl_tcb[krnl_current_task];
	rc = _context_save(krnl_task->task_context);
	if (rc)
		return;
	if (krnl_task->state == TASK_RUNNING)
		krnl_task->state = TASK_READY;
	if (krnl_task->pstack[0] != STACK_MAGIC)
		panic(PANIC_STACK_OVERFLOW);
	if (krnl_tasks > 0){
		process_delay_queue();
		krnl_current_task = krnl_pcb.sched_rt();
		if (krnl_current_task == 0)
			krnl_current_task = krnl_pcb.sched_apr();
		if (krnl_current_task == 0)
			krnl_current_task = krnl_pcb.sched_be();
		krnl_task->state = TASK_RUNNING;
		krnl_pcb.preempt_cswitch++;
#if KERNEL_LOG >= 1
		dprintf("\n%d %d %d %d %d ", krnl_current_task, krnl_task->period, krnl_task->capacity, krnl_task->deadline, (uint32_t)_read_us());
#endif
		_context_restore(krnl_task->task_context, 1);
		panic(PANIC_UNKNOWN);
	}else{
		panic(PANIC_NO_TASKS_LEFT);
	}
}

void escalonator(void) {
	int32_t i, j, k;
	uint16_t id = 0;
	struct tcb_entry *e1, *e2;
	
	k = hf_queue_count(krnl_aperiodic_tasks_queue);
	if (k == 0)
		return 0;
		
	for (i = 0; i < k-1; i++){
		for (j = i + 1; j < k; j++){
			e1 = hf_queue_get(krnl_rt_queue, i);
			e2 = hf_queue_get(krnl_rt_queue, j);
			if (e1->period > e2->period)
				if (hf_queue_swap(krnl_rt_queue, i, j))
					panic(PANIC_CANT_SWAP);
		}
	}

	for (i = 0; i < k; i++){
		rt_queue_next();
		if (krnl_task->state != TASK_BLOCKED && krnl_task->capacity_rem > 0 && !id){
			id = krnl_task->id;
			--krnl_task->capacity_rem;
		}
		if (--krnl_task->deadline_rem == 0){
			krnl_task->deadline_rem = krnl_task->period;
			if (krnl_task->capacity_rem > 0) krnl_task->deadline_misses++;
			krnl_task->capacity_rem = krnl_task->capacity;
		}
	}

	if (id){
		krnl_task = &krnl_tcb[id];
		krnl_task->rtjobs++;
		return id;
	}else{
		/* no RT task to run */
		krnl_task = &krnl_tcb[0];
		return 0;
	}
}
