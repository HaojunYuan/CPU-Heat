#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <cstring>
#include <list>
#include <vector>
#include <queue>
#include <unordered_map>
#include <iostream>
// #define DEBUG 1

#include "procsim.hpp"
using namespace std;

//
// TODO: Define any useful data structures and functions here
//
size_t f;
size_t s;
size_t p;
size_t a;
size_t l;
size_t m;
size_t sqMaxSize;
size_t retired_prev_cycle = 0;
size_t freePregs = 0;
struct reg
{
    bool ready;
    bool free;
};
struct ReservationStation
{
    opcode_t opcode; // Functional unit
    uint8_t dest;
    uint8_t src1;
    uint8_t src2;
    uint64_t ls_address;
    uint64_t instruction_count;
    bool icache_miss;
    bool dcache_miss;
    bool scheduled;
};
struct rob_entry
{
    uint8_t Dest_Areg; // Destination architectural register
    uint8_t Prev_Preg; // Previous physical register
    size_t index;
    opcode_t inst; // Functional unit
    bool mispredict;
    bool ready;     // Indicates whether the instruction is ready or not
    bool exception; // Indicates whether the instruction caused an exception or not
};
list<inst_t> dq;
vector<ReservationStation> sq;
unordered_map<uint64_t, uint64_t> rat;
vector<reg> prf;
vector<rob_entry> rob;
size_t rob_entries;
struct alu
{
    bool free = 1;
    uint8_t dest;
    size_t index;
};
vector<alu> alu_units;
// 3 Stage multiplier
struct mul_entry
{
    uint8_t dest;
    int cycles_remaining;
    size_t index;
};
struct mul
{
    bool free = 1;
    vector<mul_entry> mul_entries;
};
vector<mul> mul_units;
struct lsu
{
    bool free = 1;
    bool load = 0;
    bool dcache_miss;
    uint64_t ls_address;
    uint8_t dest;
    size_t index;
};
vector<lsu> lsu_units;
vector<uint64_t> store_buffer;
vector<int> load_cycles;
uint64_t total_dq_usage = 0;
uint64_t total_sq_usage = 0;
uint64_t total_rob_usage = 0;

// The helper functions in this#ifdef are optional and included here for your
// convenience so you can spend more time writing your simulator logic and less
// time trying to match debug trace formatting! (If you choose to use them)
#ifdef DEBUG
// TODO: Fix the debug outs
static void
print_operand(int8_t rx)
{
    if (rx < 0)
    {
        printf("(none)"); //  PROVIDED
    }
    else
    {
        printf("R%" PRId8, rx); //  PROVIDED
    }
}

// Useful in the fetch and dispatch stages
static void print_instruction(const inst_t *inst)
{
    if (!inst)
        return;
    static const char *opcode_names[] = {NULL, NULL, "ADD", "MUL", "LOAD", "STORE", "BRANCH"};

    printf("opcode=%s, dest=", opcode_names[inst->opcode]);   //  PROVIDED
    print_operand(inst->dest);                                //  PROVIDED
    printf(", src1=");                                        //  PROVIDED
    print_operand(inst->src1);                                //  PROVIDED
    printf(", src2=");                                        //  PROVIDED
    print_operand(inst->src2);                                //  PROVIDED
    printf(", dyncount=%lu \n", inst->dyn_instruction_count); //  PROVIDED
}

// This will print out the state of the RAT
static void print_rat(void)
{
    for (uint64_t regno = 0; regno < NUM_REGS; regno++)
    {
        if (regno == 0)
        {
            printf("    { R%02" PRIu64 ": P%03" PRIu64 " }", regno, rat[regno]); // TODO: fix me
        }
        else if (!(regno & 0x3))
        {
            printf("\n    { R%02" PRIu64 ": P%03" PRIu64 " }", regno, rat[regno]); //  TODO: fix me
        }
        else
        {
            printf(", { R%02" PRIu64 ": P%03" PRIu64 " }", regno, rat[regno]); //  TODO: fix me
        }
    }
    printf("\n"); //  PROVIDED
}

// This will print out the state of the register file, where P0-P31 are architectural registers
// and P32 is the first PREG
static void print_prf(void)
{
    for (uint64_t regno = 0; regno < prf.size(); regno++)
    { // TODO: fix me
        if (regno == 0)
        {
            printf("    { P%03" PRIu64 ": Ready: %d, Free: %d }", regno, prf[regno].ready, prf[regno].free); // TODO: fix me
        }
        else if (!(regno & 0x3))
        {
            printf("\n    { P%03" PRIu64 ": Ready: %d, Free: %d }", regno, prf[regno].ready, prf[regno].free); // TODO: fix me
        }
        else
        {
            printf(", { P%03" PRIu64 ": Ready: %d, Free: %d }", regno, prf[regno].ready, prf[regno].free); // TODO: fix me
        }
    }
    printf("\n"); //  PROVIDED
}

// This will print the state of the ROB where instructions are identified by their dyn_instruction_count
static void print_rob(void)
{
    size_t printed_idx = 0;
    printf("\tAllocated Entries in ROB: %lu\n", rob.size()); // TODO: Fix Me
    for (/* ??? */; /* ??? */ false; /* ??? */)
    { // TODO: Fix Me
        if (printed_idx == 0)
        {
            printf("    { dyncount=%05" PRIu64 ", completed: %d, mispredict: %d }", rob[printed_idx].index, rob[printed_idx].ready, rob[printed_idx].mispredict); // TODO: Fix Me
        }
        else if (!(printed_idx & 0x3))
        {
            printf("\n    { dyncount=%05" PRIu64 ", completed: %d, mispredict: %d }", rob[printed_idx].index, rob[printed_idx].ready, rob[printed_idx].mispredict); // TODO: Fix Me
        }
        else
        {
            printf(", { dyncount=%05" PRIu64 " completed: %d, mispredict: %d }", rob[printed_idx].index, rob[printed_idx].ready, rob[printed_idx].mispredict); // TODO: Fix Me
        }
        printed_idx++;
    }
    if (!printed_idx)
    {
        printf("    (ROB empty)"); //  PROVIDED
    }
    printf("\n"); //  PROVIDED
}
#endif

// Optional helper function which pops previously retired store buffer entries
// and pops instructions from the head of the ROB. (In a real system, the
// destination register value from the ROB would be written to the
// architectural registers, but we have no register values in this
// simulation.) This function returns the number of instructions retired.
// Immediately after retiring a mispredicting branch, this function will set
// *retired_mispredict_out = true and will not retire any more instructions.
// Note that in this case, the mispredict must be counted as one of the retired instructions.
static uint64_t stage_state_update(procsim_stats_t *stats,
                                   bool *retired_mispredict_out)
{
    // TODO: fill me in
#ifdef DEBUG
    printf("Stage Retire: \n"); //  PROVIDED
#endif
    int store_retired = 0;
    int retired = 0;
    // Pop off entries from the store buffer
    if (store_buffer.size() <= retired_prev_cycle)
    {
#ifdef DEBUG
        for (size_t i = 0; i < store_buffer.size(); i++)
        {
            printf("Address popped: %#010lx \n", store_buffer[i]);
        }
#endif
        store_buffer.clear();
    }
    else
    {
#ifdef DEBUG
        for (size_t i = 0; i < retired_prev_cycle; i++)
        {
            printf("Address popped: %#010lx \n", store_buffer[i]);
        }
#endif
        store_buffer.erase(store_buffer.begin(), store_buffer.begin() + retired_prev_cycle);
    }
    // Pop off entries from the ROB
    for (size_t i = 0; i < rob.size(); i++)
    {
        if (!rob[i].ready)
        {
            break;
        }
        else
        {
            if (rob[i].inst == opcode_t::OPCODE_STORE)
                store_retired++;
            retired++;
            stats->instructions_retired++;
#ifdef DEBUG
            printf("Instruction retired: %d \n", (int)rob[i].index);
#endif

            if (rob[i].Prev_Preg >= NUM_REGS and rob[i].Prev_Preg != 255)
            {
#ifdef DEBUG
                printf("Freeing Preg: %d \n", rob[i].Prev_Preg);
#endif
                prf[rob[i].Prev_Preg].free = 1;
                freePregs++;
            }
            if (rob[i].mispredict)
            {
                *retired_mispredict_out = true;
                break;
            }
        }
    }
    rob.erase(rob.begin(), rob.begin() + retired);
    retired_prev_cycle = store_retired;
    return retired;
}

// Optional helper function which is responsible for moving instructions
// through pipelined Function Units and then when instructions complete (that
// is, when instructions are in the final pipeline stage of an FU and aren't
// stalled there), setting the ready bits in the register file. This function
// should remove an instruction from the scheduling queue when it has completed.
static void stage_exec(procsim_stats_t *stats)
{
    // TODO: fill me in
#ifdef DEBUG
    printf("Stage Exec: \n"); //  PROVIDED
    int free_alu = 0;
    int free_mul = 0;
    int free_lsu = 0;
    for (size_t i = 0; i < a; i++)
    {
        if (alu_units[i].free)
            free_alu++;
    }
    for (size_t i = 0; i < m; i++)
    {
        if (mul_units[i].mul_entries.size() < 3)
            free_mul++;
    }
    for (size_t i = 0; i < l; i++)
    {
        if (lsu_units[i].free)
            free_lsu++;
    }
    printf("Free ALU: %d, Free MUL: %d, Free LSU: %d \n", free_alu, free_mul, free_lsu);
#endif
    auto mark_rob_ready = [](uint64_t idx)
    {
        for (size_t j = 0; j < rob.size(); j++)
        {
            if (rob[j].index == idx)
            {
                rob[j].ready = 1;
                break;
            }
        }
    };
    // Progress ALUs
#ifdef DEBUG
    printf("Progressing ALU units\n"); // PROVIDED
#endif
    for (size_t i = 0; i < a; i++)
    {
        if (alu_units[i].free)
            continue;
        if (alu_units[i].dest != 255)
            prf[alu_units[i].dest].ready = 1;
        alu_units[i].free = 1;
#ifdef DEBUG
        printf("Completing instruction: %d\n", (int)alu_units[i].index);
        printf("Freeing Preg: %d\n", alu_units[i].dest);
#endif
        mark_rob_ready(alu_units[i].index);
    }

    // Progress MULs
#ifdef DEBUG
    printf("Progressing MUL units\n"); // PROVIDED
#endif
    for (size_t i = 0; i < m; i++)
    {
        if (mul_units[i].mul_entries.empty())
        {
            continue;
        }
        // Iterate through the list of each mul
        for (size_t j = 0; j < mul_units[i].mul_entries.size(); j++)
        {
            mul_units[i].mul_entries[j].cycles_remaining--;
        }
        if (mul_units[i].mul_entries[0].cycles_remaining == 0)
        {
#ifdef DEBUG
            printf("Completing instruction: %d\n", (int)mul_units[i].mul_entries[0].index);
#endif
            if (mul_units[i].mul_entries[0].dest != 255)
                prf[mul_units[i].mul_entries[0].dest].ready = 1;

            mark_rob_ready(mul_units[i].mul_entries[0].index);
            mul_units[i].mul_entries.erase(mul_units[i].mul_entries.begin());
        }
        mul_units[i].free = 1;
    }
#ifdef DEBUG
    printf("Progressing LSU units for loads\n"); // PROVIDED
#endif
    // Progress LSU loads
    for (size_t i = 0; i < l; i++)
    {
        if (lsu_units[i].load and !lsu_units[i].free)
        {
            load_cycles[i]++;
            if (load_cycles[i] == 1)
            {
                for (size_t j = 0; j < store_buffer.size(); j++)
                {
                    if (lsu_units[i].ls_address == store_buffer[j])
                    {
#ifdef DEBUG
                        printf("Completing instruction: %d\n", (int)lsu_units[i].index);
#endif
                        stats->store_buffer_read_hits++;
                        lsu_units[i].free = 1;
                        if (lsu_units[i].dest != 255)
                            prf[lsu_units[i].dest].ready = 1;
                        load_cycles[i] = 0;
                        mark_rob_ready(lsu_units[i].index);
                        break;
                    }
                }
            }
            else if (load_cycles[i] == 2)
            {
                stats->dcache_reads++;
                if (!lsu_units[i].dcache_miss)
                {
#ifdef DEBUG
                    printf("Completing instruction: %d\n", (int)lsu_units[i].index);
#endif
                    stats->dcache_read_hits++;
                    lsu_units[i].free = 1;
                    if (lsu_units[i].dest != 255)
                        prf[lsu_units[i].dest].ready = 1;
                    load_cycles[i] = 0;
                    mark_rob_ready(lsu_units[i].index);
                }
                else
                {
                    stats->dcache_read_misses++;
                }
            }
            else if (load_cycles[i] == 2 + L1_MISS_PENALTY)
            {
#ifdef DEBUG
                printf("Completing instruction: %d\n", (int)lsu_units[i].index);
#endif
                lsu_units[i].free = 1;
                if (lsu_units[i].dest != 255)
                    prf[lsu_units[i].dest].ready = 1;
                load_cycles[i] = 0;
                mark_rob_ready(lsu_units[i].index);
            }
        }
        else if (!lsu_units[i].load and !lsu_units[i].free)
        {
            // Progress LSU stores
#ifdef DEBUG
            printf("Progressing LSU units for stores\n"); // PROVIDED
#endif
            store_buffer.push_back(lsu_units[i].ls_address);
#ifdef DEBUG
            printf("Address pushed: %#010lx \n", lsu_units[i].ls_address);
#endif
            lsu_units[i].free = 1;
            mark_rob_ready(lsu_units[i].index);
        }
    }

// Apply Result Busses
#ifdef DEBUG
    printf("Processing Result Busses\n"); // PROVIDED
#endif
    // Remove rs from scheduling queue where their destination registers are ready
    vector<ReservationStation> newSq;
    for (size_t i = 0; i < sq.size(); i++)
    {
        for (size_t j = 0; j < rob.size(); j++)
        {
            if (sq[i].instruction_count == rob[j].index and !rob[j].ready)
            {
                newSq.push_back(sq[i]);
                break;
            }
#ifdef DEBUG
            else if (sq[i].instruction_count == rob[j].index and rob[j].ready)
            {
                printf("Instruction %d ready \n", (int)sq[i].instruction_count);
            }
#endif
        }
    }
    sq = newSq;
}

// Optional helper function which is responsible for looking through the
// scheduling queue and firing instructions that have their source pregs
// marked as ready. Note that when multiple instructions are ready to fire
// in a given cycle, they must be fired in program order.
// Also, load and store instructions must be fired according to the
// memory disambiguation algorithm described in the assignment PDF. Finally,
// instructions stay in their reservation station in the scheduling queue until
// they complete (at which point stage_exec() above should free their RS).
static void stage_schedule(procsim_stats_t *stats)
{
    // TODO: fill me in
#ifdef DEBUG
    printf("Stage Schedule: \n"); //  PROVIDED
#endif
    // Handle alu and mul units
    bool fired_store = false;
    int scheduled = 0;
    for (size_t i = 0; i < sq.size(); i++)
    {
        ReservationStation rs = sq[i];
        if (rs.scheduled)
        {
            continue;
        }
#ifdef DEBUG
        else
        {
            printf("Scheduling Instruction %d! \n", (int)rs.instruction_count);
        }
#endif
        if ((rs.src1 != 255 and !prf[rs.src1].ready) or (rs.src2 != 255 and !prf[rs.src2].ready))
        {
            continue;
        }
#ifdef DEBUG
        printf("Ready Src 1: %d\n", (int)rs.src1);
        printf("Ready Src 2: %d\n", (int)rs.src2);
#endif
        if (rs.opcode == opcode_t::OPCODE_MUL)
        {
            for (size_t j = 0; j < m; j++)
            {
                if (mul_units[j].free)
                {
                    sq[i].scheduled = 1;
                    mul_units[j].free = 0;
                    mul_units[j].mul_entries.push_back({rs.dest, 3, rs.instruction_count});
                    scheduled++;
                    break;
                }
            }
        }
        else if (rs.opcode == opcode_t::OPCODE_ADD or rs.opcode == opcode_t::OPCODE_BRANCH)
        {
            for (size_t k = 0; k < a; k++)
            {
                if (alu_units[k].free)
                {
                    sq[i].scheduled = 1;
                    alu_units[k].free = 0;
                    alu_units[k].dest = rs.dest;
                    alu_units[k].index = rs.instruction_count;
                    scheduled++;
                    break;
                }
            }
        }
        // Handle load
        else if (rs.opcode == opcode_t::OPCODE_LOAD)
        {
            bool canFire = 1;
            for (size_t j = 0; j < sq.size(); j++)
            {
                if (j == i)
                {
                    continue;
                }
                ReservationStation rs2 = sq[j];
                if ((rs2.opcode == opcode_t::OPCODE_STORE) and (rs2.instruction_count < rs.instruction_count))
                {
                    canFire = 0;
                }
            }
            if (canFire)
            {
                for (size_t k = 0; k < l; k++)
                {
                    if (lsu_units[k].free)
                    {
                        sq[i].scheduled = 1;
                        lsu_units[k].free = 0;
                        lsu_units[k].load = 1;
                        lsu_units[k].dest = rs.dest;
                        lsu_units[k].dcache_miss = rs.dcache_miss;
                        lsu_units[k].ls_address = rs.ls_address;
                        lsu_units[k].index = rs.instruction_count;
                        scheduled++;
                        break;
                    }
                }
            }
        }
        else if (rs.opcode == opcode_t::OPCODE_STORE)
        {
            bool canFire = 1;
            for (size_t j = 0; j < sq.size(); j++)
            {
                if (j == i)
                {
                    continue;
                }
                ReservationStation rs2 = sq[j];
                if ((((rs2.opcode == opcode_t::OPCODE_LOAD) or (rs2.opcode == opcode_t::OPCODE_STORE)) and (rs2.instruction_count < rs.instruction_count)) or (fired_store))
                {
                    canFire = 0;
                }
            }
            if (canFire)
            {
                for (size_t k = 0; k < l; k++)
                {
                    if (lsu_units[k].free)
                    {
                        sq[i].scheduled = 1;
                        lsu_units[k].free = 0;
                        lsu_units[k].load = 0;
                        lsu_units[k].ls_address = rs.ls_address;
                        lsu_units[k].index = rs.instruction_count;
                        fired_store = true;
                        scheduled++;
                        break;
                    }
                }
            }
        }
    }
    if (scheduled == 0)
    {
        stats->no_fire_cycles++;
    }
#ifdef DEBUG
    printf("Scheduled: %d\n", scheduled);
#endif
}

// Optional helper function which looks through the dispatch queue, decodes
// instructions, and inserts them into the scheduling queue. Dispatch should
// not add an instruction to the scheduling queue unless there is space for it
// in the scheduling queue and the ROB and a free preg exists if necessary;
// effectively, dispatch allocates pregs, reservation stations and ROB space for
// each instruction dispatched and stalls if there any are unavailable.
// You will also need to update the RAT if need be.
// Note the scheduling queue has a configurable size and the ROB has P+32 entries.
// The PDF has details.
static void stage_dispatch(procsim_stats_t *stats)
{
    // TODO: fill me in
#ifdef DEBUG
    printf("Stage Dispatch: \n"); //  PROVIDED
#endif
    while (!dq.empty() && sq.size() < sqMaxSize)
    {
        if (rob.size() >= rob_entries)
        {
            stats->rob_stall_cycles += 1;
            break;
        }

        inst_t inst = dq.front();
        if (freePregs == 0 && inst.dest != -1)
        {
            stats->no_dispatch_pregs_cycles += 1;
            break;
        }
        // #ifdef DEBUG
        //             printf("Dispatching Inst %d: \n", (int)inst.dyn_instruction_count); //  PROVIDED
        // #endif
        dq.pop_front();
        ReservationStation rs;
        rob_entry r;
        rs.opcode = inst.opcode;
        rs.instruction_count = inst.dyn_instruction_count;
        rs.ls_address = inst.load_store_addr;
        rs.icache_miss = inst.icache_miss;
        rs.dcache_miss = inst.dcache_miss;
        rs.scheduled = 0;
        if (inst.src1 == -1)
        {
            rs.src1 = 255;
        }
        else
        {
            rs.src1 = rat[inst.src1];
        }
        if (inst.src2 == -1)
        {
            rs.src2 = 255;
        }
        else
        {
            rs.src2 = rat[inst.src2];
        }
        r.Dest_Areg = inst.dest;
        r.mispredict = inst.mispredict;
        r.index = inst.dyn_instruction_count;
        if (inst.dest == -1)
        {
            r.Prev_Preg = 255;
        }
        else
        {
            r.Prev_Preg = rat[inst.dest];
        }
        r.inst = inst.opcode;
        // Find a free preg
        if (inst.dest != -1)
        {
            for (size_t i = NUM_REGS; i < p + NUM_REGS; i++)
            {
                if (prf[i].free)
                {
                    // Debug
                    rat[inst.dest] = i;
                    prf[i].free = 0;
                    freePregs--;
                    break;
                }
            }
            rs.dest = rat[inst.dest];
            prf[rat[inst.dest]].ready = false;
        }
        else
        {
            rs.dest = 255;
        }
        r.ready = false;
        r.exception = false;
        sq.push_back(rs);
        rob.push_back(r);
    }
}

// Optional helper function which fetches instructions from the instruction
// cache using the provided procsim_driver_read_inst() function implemented
// in the driver and appends them to the dispatch queue. To simplify the
// project, the dispatch queue is infinite in size.
static void stage_fetch(procsim_stats_t *stats)
{
    // TODO: fill me in
#ifdef DEBUG
    printf("Stage Fetch: \n"); //  PROVIDED
#endif
    for (size_t i = 0; i < f; i++)
    {
        const inst_t *inst = procsim_driver_read_inst();
        if (inst)
        {
#ifdef DEBUG
            printf("Fetching instruction: %d \n", (int)inst->dyn_instruction_count);

#endif
            // print_instruction(inst);
            dq.push_back(*inst);
            stats->instructions_fetched++;
            if (inst->icache_miss)
            {
                stats->icache_misses++;
            }
        }
#ifdef DEBUG
        else
        {
            printf("Fetched NOP \n");
        }
#endif
    }
}

// Use this function to initialize all your data structures, simulator
// state, and statistics.
void procsim_init(const procsim_conf_t *sim_conf, procsim_stats_t *stats)
{
    // TODO: fill me in
    f = sim_conf->fetch_width;
    s = sim_conf->num_schedq_entries_per_fu;
    p = sim_conf->num_pregs;
    a = sim_conf->num_alu_fus;
    l = sim_conf->num_lsu_fus;
    m = sim_conf->num_mul_fus;
    rob_entries = sim_conf->num_rob_entries;
    sqMaxSize = s * (a + l + m);
    freePregs = p;
    for (size_t i = 0; i < NUM_REGS; i++)
    {
        rat[i] = i;
        prf.push_back(reg{1, 0});
    }
    for (size_t j = NUM_REGS; j < p + NUM_REGS; j++)
    {
        prf.push_back(reg{0, 1});
    }
    // Initialize alu_units
    for (size_t i = 0; i < a; i++)
    {
        alu_units.push_back(alu{});
    }
    for (size_t i = 0; i < l; i++)
    {
        lsu_units.push_back(lsu{});
        load_cycles.push_back(0);
    }
    for (size_t i = 0; i < m; i++)
    {
        mul_units.push_back(mul{});
    }

#ifdef DEBUG
    printf("\nScheduling queue capacity: %lu instructions\n", sqMaxSize); // TODO: Fix ME
    printf("Initial RAT state:\n");                                       //  PROVIDED
    print_rat();
    printf("\n"); //  PROVIDED
#endif
}

// To avoid confusion, we have provided this function for you. Notice that this
// calls the stage functions above in reverse order! This is intentional and
// allows you to avoid having to manage pipeline registers between stages by
// hand. This function returns the number of instructions retired, and also
// returns if a mispredict was retired by assigning true or false to
// *retired_mispredict_out, an output parameter.
uint64_t procsim_do_cycle(procsim_stats_t *stats,
                          bool *retired_mispredict_out)
{
#ifdef DEBUG
    printf("================================ Begin cycle %" PRIu64 " ================================\n", stats->cycles); //  PROVIDED
#endif

    // stage_state_update() should set *retired_mispredict_out for us
    uint64_t retired_this_cycle = stage_state_update(stats, retired_mispredict_out);

    if (*retired_mispredict_out)
    {
#ifdef DEBUG
        printf("%" PRIu64 " instructions retired. Retired mispredict, so notifying driver to fetch correctly!\n", retired_this_cycle); //  PROVIDED
#endif

        // After we retire a misprediction, the other stages don't need to run
        stats->branch_mispredictions++;
    }
    else
    {
#ifdef DEBUG
        printf("%" PRIu64 " instructions retired. Did not retire mispredict, so proceeding with other pipeline stages.\n", retired_this_cycle); //  PROVIDED
#endif

        // If we didn't retire an interupt, then continue simulating the other
        // pipeline stages
        stage_exec(stats);
        stage_schedule(stats);
        stage_dispatch(stats);
        stage_fetch(stats);
    }

#ifdef DEBUG
    printf("End-of-cycle dispatch queue usage: %lu\n", dq.size()); // TODO: Fix Me
    printf("End-of-cycle sched queue usage: %lu\n", sq.size());    // TODO: Fix Me
    printf("End-of-cycle ROB usage: %lu\n", rob.size());           // TODO: Fix Me
    printf("End-of-cycle RAT state:\n");                           //  PROVIDED
    print_rat();
    printf("End-of-cycle Physical Register File state:\n"); //  PROVIDED
    print_prf();
    printf("End-of-cycle ROB state:\n"); //  PROVIDED
    print_rob();
    printf("================================ End cycle %" PRIu64 " ================================\n", stats->cycles); //  PROVIDED
    if (stats->cycles == 260600)
    {
        printf("Debugging...\n");
    }
    else if (stats->cycles == 260700)
    {
        printf("Debugging...\n");
    }
    print_instruction(NULL);
    // this makes the compiler happy, ignore it
#endif
    // TODO: Increment max_usages and avg_usages in stats here!
    stats->cycles++;
    stats->dispq_max_size = dq.size() > stats->dispq_max_size ? dq.size() : stats->dispq_max_size;
    stats->schedq_max_size = sq.size() > stats->schedq_max_size ? sq.size() : stats->schedq_max_size;
    stats->rob_max_size = rob.size() > stats->rob_max_size ? rob.size() : stats->rob_max_size;
    total_dq_usage += dq.size();
    total_sq_usage += sq.size();
    total_rob_usage += rob.size();
    // Return the number of instructions we retired this cycle (including the
    // interrupt we retired, if there was one!)
    return retired_this_cycle;
}

// Use this function to free any memory allocated for your simulator and to
// calculate some final statistics.
void procsim_finish(procsim_stats_t *stats)
{
    // TODO: fill me in
    stats->ipc = (double)stats->instructions_retired / stats->cycles;
    stats->dispq_avg_size = (double)total_dq_usage / stats->cycles;
    stats->schedq_avg_size = (double)total_sq_usage / stats->cycles;
    stats->rob_avg_size = (double)total_rob_usage / stats->cycles;
    // L2 miss ratio
    stats->dcache_read_miss_ratio = (double)stats->dcache_read_misses / stats->dcache_reads;
    // L1 hit ratio
    stats->store_buffer_hit_ratio = (double)stats->store_buffer_read_hits / (stats->store_buffer_read_hits + stats->dcache_reads);
    // L2 read time
    stats->dcache_read_aat = L1_HIT_TIME + stats->dcache_read_miss_ratio * L1_MISS_PENALTY;
    stats->read_aat = 1 * stats->store_buffer_hit_ratio + (1 - stats->store_buffer_hit_ratio) * stats->dcache_read_aat;
}
