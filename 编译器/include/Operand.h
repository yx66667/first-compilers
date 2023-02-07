#ifndef __OPERAND_H__
#define __OPERAND_H__

#include "SymbolTable.h"
#include <vector>

class Instruction;
class Function;


// class Operand - The operand of an instruction.
class Operand
{
typedef std::vector<Instruction *>::iterator use_iterator;

private:
    Instruction *def;                // The instruction where this operand is defined.
    std::vector<Instruction *> uses; // Intructions that use this operand.
    SymbolEntry *se;                 // The symbol entry of this operand.
    bool is_param; //用来判断是不是参数：这里主要用于在Instruction.cpp中生genMachineOperand函数中使用
public:
    Operand(SymbolEntry*se) :se(se){def = nullptr; this->is_param = false;};
    void setDef(Instruction *inst) {def = inst;};
    void addUse(Instruction *inst) { uses.push_back(inst);};
    void removeUse(Instruction *inst);
    int usersNum() const {return uses.size();};

    use_iterator use_begin() {return uses.begin();};
    use_iterator use_end() {return uses.end();};
    Type* getType() {return se->getType();};
    std::string toStr() const;
    SymbolEntry * getEntry() { return se; };
    Instruction* getDef() { return def; };
    //添加了一行
    SymbolEntry* get_symbolentry(){return this->se;};
    void set_isParam(bool param){this->is_param = param;};
    bool isParam() const {return is_param;};
};

#endif