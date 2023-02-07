#include <algorithm>
#include "LinearScan.h"
#include "MachineCode.h"
#include "LiveVariableAnalysis.h"

LinearScan::LinearScan(MachineUnit *unit)
{
    this->unit = unit;
    for (int i = 4; i < 11; i++)
        regs.push_back(i);
}

void LinearScan::allocateRegisters() 
{
    for (auto &f : unit->getFuncs()) //遍历每个函数 获得虚拟寄存器对应的物理寄存器
    {
        func = f;
        bool success; //用success判断何时完成分配
        success = false;
        while (!success)        // repeat until all vregs can be mapped
        {
            computeLiveIntervals(); //旧区间重构，重分配原来的寄存器
            success = linearScanRegisterAllocation();
            if (success)        // all vregs can be mapped to real regs
                modifyCode();  //没有溢出 修改寄存器映射即可退出
            else                // spill vregs that can't be mapped to real regs
                genSpillCode(); //生成溢出代码 然后继续循环
        }
    }
}

void LinearScan::makeDuChains()
{
    LiveVariableAnalysis lva;
    lva.pass(func);
    du_chains.clear();
    int i = 0;
    std::map<MachineOperand, std::set<MachineOperand *>> liveVar;
    for (auto &bb : func->getBlocks())
    {
        liveVar.clear();
        for (auto &t : bb->getLiveOut())
            liveVar[*t].insert(t);
        int no;
        no = i = bb->getInsts().size() + i;
        for (auto inst = bb->getInsts().rbegin(); inst != bb->getInsts().rend(); inst++)
        {
            (*inst)->setNo(no--);
            for (auto &def : (*inst)->getDef())
            {
                if (def->isVReg())
                {
                    auto &uses = liveVar[*def];
                    du_chains[def].insert(uses.begin(), uses.end());
                    auto &kill = lva.getAllUses()[*def];
                    std::set<MachineOperand *> res;
                    set_difference(uses.begin(), uses.end(), kill.begin(), kill.end(), inserter(res, res.end()));
                    liveVar[*def] = res;
                }
            }
            for (auto &use : (*inst)->getUse())
            {
                if (use->isVReg())
                    liveVar[*use].insert(use);
            }
        }
    }
}

void LinearScan::computeLiveIntervals()  //旧区间重构，重分配原来的寄存器
{
    makeDuChains();
    intervals.clear();
    for (auto &du_chain : du_chains)
    {
        int t = -1;
        for (auto &use : du_chain.second)
            t = std::max(t, use->getParent()->getNo());
        Interval *interval = new Interval({du_chain.first->getParent()->getNo(), t, false, 0, 0, {du_chain.first}, du_chain.second});
        intervals.push_back(interval);
    }
    for (auto& interval : intervals) {
        auto uses = interval->uses;
        auto begin = interval->start;
        auto end = interval->end;
        for (auto block : func->getBlocks()) {
            auto liveIn = block->getLiveIn();
            auto liveOut = block->getLiveOut();
            bool in = false;
            bool out = false;
            for (auto use : uses)
                if (liveIn.count(use)) {
                    in = true;
                    break;
                }
            for (auto use : uses)
                if (liveOut.count(use)) {
                    out = true;
                    break;
                }
            if (in && out) {
                begin = std::min(begin, (*(block->begin()))->getNo());
                end = std::max(end, (*(block->rbegin()))->getNo());
            } else if (!in && out) {
                for (auto i : block->getInsts())
                    if (i->getDef().size() > 0 &&
                        i->getDef()[0] == *(uses.begin())) {
                        begin = std::min(begin, i->getNo());
                        break;
                    }
                end = std::max(end, (*(block->rbegin()))->getNo());
            } else if (in && !out) {
                begin = std::min(begin, (*(block->begin()))->getNo());
                int temp = 0;
                for (auto use : uses)
                    if (use->getParent()->getParent() == block)
                        temp = std::max(temp, use->getParent()->getNo());
                end = std::max(temp, end);
            }
        }
        interval->start = begin;
        interval->end = end;
    }
    bool change;
    change = true;
    while (change)
    {
        change = false;
        std::vector<Interval *> t(intervals.begin(), intervals.end());
        for (size_t i = 0; i < t.size(); i++)
            for (size_t j = i + 1; j < t.size(); j++)
            {
                Interval *w1 = t[i];
                Interval *w2 = t[j];
                if (**w1->defs.begin() == **w2->defs.begin())
                {
                    std::set<MachineOperand *> temp;
                    set_intersection(w1->uses.begin(), w1->uses.end(), w2->uses.begin(), w2->uses.end(), inserter(temp, temp.end()));
                    if (!temp.empty())
                    {
                        change = true;
                        w1->defs.insert(w2->defs.begin(), w2->defs.end());
                        w1->uses.insert(w2->uses.begin(), w2->uses.end());
                        auto w1Min = std::min(w1->start, w1->end);
                        auto w1Max = std::max(w1->start, w1->end);
                        auto w2Min = std::min(w2->start, w2->end);
                        auto w2Max = std::max(w2->start, w2->end);
                        w1->start = std::min(w1Min, w2Min);
                        w1->end = std::max(w1Max, w2Max);
                        auto it = std::find(intervals.begin(), intervals.end(), w2);
                        if (it != intervals.end())
                            intervals.erase(it);
                    }
                }
            }
    }
    sort(intervals.begin(), intervals.end(), compareStart);
}

//遍历 intervals 列表进行寄存器分配(待完善)
bool LinearScan::linearScanRegisterAllocation()
{
    // Todo
    bool success=true;
    //initialize
    active.clear();
    regs.clear();
    //初始放入可用分配寄存器
    for (int i = 10; i >=4; i--)
        regs.push_back(i);
    //遍历 intervals 列表
    for(auto& i : intervals)
    {
        //遍历 active 列表，看该列表中是否存在结束时间早于 unhandled interval 的 interval
        expireOldIntervals(i);

        if(regs.empty())
        {
            //所有物理寄存器都被占用, 溢出
            spillAtInterval(i);
            success=false;
        }
        else{
            //为 unhandled interval 分配物理寄存器
            i->rreg = regs.front();
            regs.erase(regs.begin());
            //放入已经分配的向量中
            active.push_back(std::move(i));
            //对活跃区间按照结束时间升序排序
            sort(active.begin(), active.end(), [](Interval* a, Interval* b){return a->end < b->end;});
        }
    }
    return success;
}

void LinearScan::modifyCode()
{
    for (auto &interval : intervals)
    {
        func->addSavedRegs(interval->rreg);
        for (auto def : interval->defs)
            def->setReg(interval->rreg);
        for (auto use : interval->uses)
            use->setReg(interval->rreg);
    }
}

//生成溢出代码(待完善)
void LinearScan::genSpillCode() {
    for (auto& interval : intervals) {
        if (!interval->spill)
            continue;
        // TODO
        /* HINT:
         * The vreg should be spilled to memory.
         * 1. insert ldr inst before the use of vreg
         * 2. insert str inst after the def of vreg
         */
        //为其在栈内分配空间，获取当前在栈内相对 FP 的偏移
        interval->disp = func->AllocSpace(4);

        auto offset = new MachineOperand(MachineOperand::IMM, interval->disp);
        auto fp = new MachineOperand(MachineOperand::REG, 11);

        //1. insert ldr inst before the use of vreg
        for (auto use : interval->uses) {
            
            auto temp = new MachineOperand(*use);
            MachineOperand* operand = nullptr;
            //判断是否超出寻址空间 
            if (interval->disp > 255 || interval->disp < -255) 
            {
                //ldr v1,offset
                operand = new MachineOperand(MachineOperand::VREG, SymbolTable::getLabel());
                auto inst1 = new LoadMInstruction(use->getParent()->getParent(), operand, offset);
                use->getParent()->insertBefore(inst1); //USE指令前插入Load指令
            }
            MachineInstruction *inst=nullptr;
            
            if (operand)//超出寻址空间
            {
                //ldr r0,[fp,v1]
                inst = new LoadMInstruction(use->getParent()->getParent(), temp, fp, new MachineOperand(*operand));
            } else { //未超出寻址空间
                inst = new LoadMInstruction(use->getParent()->getParent(),temp, fp, offset);
                
            }
            use->getParent()->insertBefore(inst);
        }
        //2. insert str inst after the def of vreg
        for (auto def : interval->defs) {

            auto temp = new MachineOperand(*def);
            MachineOperand* operand = nullptr;
            MachineInstruction *inst1 = nullptr, *inst2 = nullptr;
            
            //判断是否超出寻址空间 
            if (interval->disp > 255 || interval->disp < -255) 
            {
                operand = new MachineOperand(MachineOperand::VREG, SymbolTable::getLabel());
                inst1 = new LoadMInstruction(def->getParent()->getParent(),operand, offset);
                def->getParent()->insertAfter(inst1);
            }

            if (operand)
                inst2 = new StoreMInstruction(def->getParent()->getParent(), temp, fp, new MachineOperand(*operand));
            else
                inst2 = new StoreMInstruction(def->getParent()->getParent(), temp, fp, offset);
            
            if (inst1)
                inst1->insertAfter(inst2);
            else
                def->getParent()->insertAfter(inst2);
        }
    }
}

//遍历 active 列表，看该列表中是否存在结束时间早于 unhandled interval 的 interval
void LinearScan::expireOldIntervals(Interval *interval)
{
    // Todo
    auto temp=active.begin();
    while(temp!=active.end())
    {
        if((*temp)->end>=interval->start)
        {
            return;
        }
        regs.push_back((*temp)->rreg);
        temp=active.erase(find(active.begin(),active.end(),*temp));
        sort(regs.begin(),regs.end());
    }
}

//在active列表中最后一个interval和当前unhandled interval中选择一个interval将其溢出到栈中
void LinearScan::spillAtInterval(Interval *interval)
{
    // Todo
    
    auto lastIt=active[active.size()-1];
    if(lastIt->end<=interval->end)//unhandled interval 的结束时间更晚
    {
        interval->spill = true;
    }
    else{//active 列表中的 interval 结束时间更晚
        lastIt->spill = true;
        //将其占用的寄存器分配给 unhandled interval
        interval->rreg=lastIt->rreg;
        func->addSavedRegs(interval->rreg);
        active.push_back(std::move(interval));
        sort(active.begin(),active.end(),[](Interval* a, Interval* b){return a->end < b->end;});
    }
}

bool LinearScan::compareStart(Interval *a, Interval *b)
{
    return a->start < b->start;
}