#include "Ast.h"
#include "SymbolTable.h"
#include "Unit.h"
#include "Instruction.h"
#include "IRBuilder.h"
#include <iostream>
#include <string>
#include "Type.h"
#include <assert.h>
extern MachineUnit mUnit;

extern FILE *yyout;
int Node::counter = 0;
IRBuilder* Node::builder = nullptr;
Type* returnType = nullptr; //函数定义时返回的类型
bool funcReturned = false; //用于类型检查时看函数体内有没有return 语句
int inIteration = 0; //inIteration是循环的层数
int genBr = 0;

Node::Node()
{
    seq = counter++;
}
//将list中所有指令的跳转地址设置为bb 条件跳转就将为真情况跳到bb
void Node::backPatch(std::vector<Instruction*> &list, BasicBlock*bb)
{
    for(auto &inst:list)
    {
        if(inst->isCond())
            dynamic_cast<CondBrInstruction*>(inst)->setTrueBranch(bb);
        else if(inst->isUncond())
            dynamic_cast<UncondBrInstruction*>(inst)->setBranch(bb);
    }
}

std::vector<Instruction*> Node::merge(std::vector<Instruction*> &list1, std::vector<Instruction*> &list2)
{//将两个列表进行合并
    std::vector<Instruction*> res(list1);
    res.insert(res.end(), list2.begin(), list2.end());
    return res;
}

Type* ExprNode::getType()
{
    return symbolEntry->getType();
}

void Ast::genCode(Unit *unit)
{
    IRBuilder *builder = new IRBuilder(unit);
    Node::setIRBuilder(builder);
    root->genCode();
    /*
    fprintf(yyout,"declare void @putint(i32)\n");
    fprintf(yyout,"declare void @putch(i32)\n");
    fprintf(yyout,"declare i32 @getint() \n");
    fprintf(yyout,"declare i32 @getch()\n");
    */
}
//新写的，还需完善
void FunctionDef::genCode()
{
    Unit *unit = builder->getUnit();
    Function *func = new Function(unit, se);
    BasicBlock *entry = func->getEntry();
    // set the insert point to the entry basicblock of this function.
    builder->setInsertBB(entry);

    if(params!=nullptr){
        params->genCode();
    }
    stmt->genCode();

    /**
     * Construct control flow graph. You need do set successors and predecessors for each basic block.
     * Todo
    */
    fprintf(stderr,"函数定义\n");
    for (auto it = func->begin(); it != func->end(); it++) 
    {
        //fprintf(stderr,"link\n");

        Instruction* index = (*it)->begin();
        while(index != (*it)->end()) {
            if(index->isRet()) {
                while(index != (*it)->rbegin()) {
                    (*it)->remove(index->getNext());
                }
                break;
            }
            index = index->getNext();
        }

        Instruction * ptr = (*it)->rbegin();
        if(ptr->isCond())
        {
            fprintf(stderr,"isCond\n");
            BasicBlock * truebb = dynamic_cast<CondBrInstruction*>(ptr)->getTrueBranch();
            BasicBlock * falsebb = dynamic_cast<CondBrInstruction*>(ptr)->getFalseBranch();
            (*it)->addSucc(truebb); (*it)->addSucc(falsebb);
            truebb->addPred(*it); falsebb->addPred(*it);
        }
        else if(ptr->isUncond())
        {
            fprintf(stderr,"isUncond\n");
            BasicBlock * next = dynamic_cast<UncondBrInstruction*>(ptr)->getBranch();
            (*it)->addSucc(next);
            next->addPred(*it);
        }
        else if ((!ptr->isRet())&&((FunctionType*)(se->getType()))->getRetType() ==TypeSystem::voidType) 
        {
            new RetInstruction(nullptr, *it);
        }
    }
}


void FuncDefParamsNode::genCode()
{
    Function *func = builder->getInsertBB()->getParent();
    BasicBlock *entry = func->getEntry();
    for(auto id : paramsList){
        Operand* idop = id->getOperand(); //获取id的操作数
        idop->set_isParam(true); //设置操作数是参数：这里主要用于在Instruction.cpp中生genMachineOperand函数中使用
        func->insertParam(idop); //插入到参数链表中
        IdentifierSymbolEntry* se = dynamic_cast<IdentifierSymbolEntry*>(id->getSymbolEntry());
        Type *type = new PointerType(id->getType()); //建立临时变量，并存入临时变量符号表
        SymbolEntry *addr_se = new TemporarySymbolEntry(type, SymbolTable::getLabel());
        Operand* addr = new Operand(addr_se);
        Instruction *allo = new AllocaInstruction(addr, se); //声明这个临时变量类型的指令
        entry->insertFront(allo);                         
        se->setAddr(addr);

        new StoreInstruction(addr, idop, entry); //如果有赋值表达式，那么需要store指令
    }
}

void DefNode::genCode()
{
    //由于只用作定义时函数参数的结点，所以什么都不用做
}

void FuncCallParamsNode::genCode()
{
    genBr--;
    for(auto expr : paramsList){
        expr->genCode();
    }
    genBr++;
}

std::vector<Operand*> FuncCallParamsNode::getOperandList()
{
    std::vector<Operand*> result;
    for(auto param : paramsList){
        result.push_back(param->getOperand());
    }
    return result;
}

void FuncCallNode::genCode()
{
    BasicBlock *bb = builder->getInsertBB();
    //没有参数
    if(params==nullptr){
        std::vector<Operand*> emptyList;
        new CallInstruction(dst, emptyList, dynamic_cast<IdentifierSymbolEntry*>(funcId->getSymPtr()), bb);
    }
    else{
        params->genCode();
        std::vector<Operand*> rParams = params->getOperandList();
        fprintf(stderr,"funccall1\n");
        new CallInstruction(dst, rParams, dynamic_cast<IdentifierSymbolEntry*>(funcId->getSymPtr()), bb);
    }
}

//新写的
void BinaryExpr::genCode()
{
    BasicBlock *bb = builder->getInsertBB();
    Function *func = bb->getParent();
    //逻辑运算
    if (op == AND) 
    {
        expr1->getOperand()->get_symbolentry()->setType(TypeSystem::boolType);
        expr2->getOperand()->get_symbolentry()->setType(TypeSystem::boolType);
        BasicBlock *trueBB = new BasicBlock(func);  // if the result of lhs is true, jump to the trueBB.
        genBr = 1;
        expr1->genCode();
        backPatch(expr1->trueList(), trueBB);
        builder->setInsertBB(trueBB);               // set the insert point to the trueBB so that intructions generated by expr2 will be inserted into it.
        expr2->genCode();
        true_list = expr2->trueList();
        false_list = merge(expr1->falseList(), expr2->falseList());
    }
    else if(op == OR)
    {
        expr1->getOperand()->get_symbolentry()->setType(TypeSystem::boolType);
        expr2->getOperand()->get_symbolentry()->setType(TypeSystem::boolType);
        BasicBlock *falseBB = new BasicBlock(func);
        genBr = 1;
        expr1->genCode();
        backPatch(expr1->falseList(), falseBB);
        builder->setInsertBB(falseBB);
        expr2->genCode();
        true_list = merge(expr1->trueList(), expr2->trueList());
        false_list = expr2->falseList();
    }
    //关系运算
    else if(op >= LESS && op <= EXCLAIMEQUAL)
    {
        genBr--;
        expr1->genCode();
        expr2->genCode();
        genBr++;
        Operand *src1 = expr1->getOperand();
        Operand *src2 = expr2->getOperand();
        int opcode;
        switch (op)
        {
        case LESS:
            opcode = CmpInstruction::L;
            break;
        case LESSEQUAL:
            opcode = CmpInstruction::LE;
            break;
        case GREATER:
            opcode = CmpInstruction::G;
            break;
        case GREATEREQUAL:
            opcode = CmpInstruction::GE;
            break;
        case EQUAL:
            opcode = CmpInstruction::E;
            break;
        case EXCLAIMEQUAL:
            opcode = CmpInstruction::NE;
            break;
        }
        new CmpInstruction(opcode, dst, src1, src2, bb);
        if(genBr > 0){
            // 跳转目标block
            BasicBlock* trueBlock, *falseBlock, *mergeBlock;
            trueBlock = new BasicBlock(func);
            falseBlock = new BasicBlock(func);
            mergeBlock = new BasicBlock(func);
            true_list.push_back(new CondBrInstruction(trueBlock, falseBlock, dst, bb));
            false_list.push_back(new UncondBrInstruction(mergeBlock, falseBlock));
        }
    }
    //算术运算
    else if(op >= ADD && op <= MOD)
    {
        expr1->genCode();
        expr2->genCode();
        Operand *src1 = expr1->getOperand();
        Operand *src2 = expr2->getOperand();
        int opcode;
        switch (op)
        {
        case ADD:
            opcode = BinaryInstruction::ADD;
            break;
        case SUB:
            opcode = BinaryInstruction::SUB;
            break;
        case MUL:
            opcode = BinaryInstruction::MUL;
            break;
        case DIV:
            opcode = BinaryInstruction::DIV;
            break;
        case MOD:
            opcode = BinaryInstruction::MOD;
            break;
        }
        new BinaryInstruction(opcode, dst, src1, src2, bb);
    }
}
//新写的
void SingleExpr::genCode()
{
    BasicBlock *bb = builder->getInsertBB();
    Function *func = bb->getParent();
    bool isCond = builder->isCond();
    if (op == SUB)
    {
        if(!isCond)
        {
            expr->genCode();
            Operand *src1;
            Operand *src2;
            src1 = new Operand(new ConstantSymbolEntry(TypeSystem::intType, 0));
            src2 = expr->getOperand();
            new BinaryInstruction(BinaryInstruction::SUB, dst, src1, src2, bb);

        }
        else{
            genBr--;
            expr->genCode();
            genBr++;
            Operand *src1;
            Operand *src2;
            src1 = new Operand(new ConstantSymbolEntry(TypeSystem::intType, 0));
            //fprintf(stderr,"Size%d\n",src1->getType()->getSize());
            if(expr->getOperand()->getType()->getSize()==1)
            {
                fprintf(stderr,"cast\n");
                //src2 = new Operand(new TemporarySymbolEntry(TypeSystem::intType, SymbolTable::getLabel()));
                // bool zext to int 
                //new ZextInstruction(expr->getOperand(), src2, bb,ZextInstruction::BoolToInt);
                src2 = expr->getOperand();
            }
            else{
                src2 = expr->getOperand();
            }
            //Operand* temp=new Operand(new TemporarySymbolEntry(TypeSystem::intType,SymbolTable::getLabel()));
            //new BinaryInstruction(BinaryInstruction::SUB, temp, src1, src2, bb);
            //dst=temp;
            new BinaryInstruction(BinaryInstruction::SUB, dst, src1, src2, bb);
            //new ZextInstruction(dst, dst, bb,ZextInstruction::IntToBool);
        }
    }
    else if(op == EXCLAIM)
    {
        genBr--;
        expr->genCode();
        genBr++;
        if(isCond)
        {
            Operand* src=expr->getOperand();
            Operand* temp=new Operand(new TemporarySymbolEntry(TypeSystem::boolType,SymbolTable::getLabel()));
            new CmpInstruction(CmpInstruction::E, temp, src, new Operand(new ConstantSymbolEntry(TypeSystem::intType,0)), bb);
            dst=temp;
        }
    }
    if(isCond&&genBr>0)
    {
        //fprintf(stderr,"cond\n");
        BasicBlock *truebb, *falsebb, *tempbb;
        truebb = new BasicBlock(func);
        falsebb = new BasicBlock(func);
        tempbb = new BasicBlock(func);
        
        true_list.push_back(new CondBrInstruction(truebb, tempbb, dst, bb));

        false_list.push_back(new UncondBrInstruction(falsebb, tempbb));
    }
}

void Constant::genCode()
{
    // we don't need to generate code.
}

void Id::genCode()
{
    BasicBlock *bb = builder->getInsertBB();
    Operand *addr = dynamic_cast<IdentifierSymbolEntry*>(symbolEntry)->getAddr();
    new LoadInstruction(dst, addr, bb);
}

void IfStmt::genCode()
{
    Function *func;
    BasicBlock *then_bb, *end_bb;
    fprintf(stderr,"ifStmt1\n");

    func = builder->getInsertBB()->getParent();
    then_bb = new BasicBlock(func);
    end_bb = new BasicBlock(func);

    genBr = 1;
    builder->setisCond(true);
    cond->genCode();
    builder->setisCond(false);
    backPatch(cond->trueList(), then_bb);
    backPatch(cond->falseList(), end_bb);

    builder->setInsertBB(then_bb);
    thenStmt->genCode();
    then_bb = builder->getInsertBB();
    new UncondBrInstruction(end_bb, then_bb);

    builder->setInsertBB(end_bb);
    fprintf(stderr,"ifStmt2\n");
}

void IfElseStmt::genCode()
{
     // Todo
    Function *func;
    BasicBlock *then_bb,*else_bb, *end_bb;

    fprintf(stderr,"ifElseStmt\n");
    func = builder->getInsertBB()->getParent();
    then_bb = new BasicBlock(func);
    else_bb= new BasicBlock(func);
    end_bb = new BasicBlock(func);

    genBr=1;
    builder->setisCond(true);
    cond->genCode();
    builder->setisCond(false);
    backPatch(cond->trueList(), then_bb);
    backPatch(cond->falseList(), else_bb);

    builder->setInsertBB(then_bb);
    thenStmt->genCode();
    then_bb=builder->getInsertBB();
    new UncondBrInstruction(end_bb, then_bb);

    builder->setInsertBB(else_bb);
    elseStmt->genCode();
    else_bb=builder->getInsertBB();
    new UncondBrInstruction(end_bb,else_bb);

    builder->setInsertBB(end_bb);
}
//新写的
void CompoundStmt::genCode()
{
    // Todo
    if(stmt!=NULL)
    {
        stmt->genCode();
    }
}

void EmptyStmt::genCode()
{
    //不用生成
}

void ExprStmtNode::genCode()
{
    for(auto expr : exprList){
        expr->genCode();
    }
}
//新写的
void SeqNode::genCode()
{
    // Todo
    stmt1->genCode();
    if(stmt2!=NULL)
    { stmt2->genCode();
    }
}

void DeclStmt::genCode()
{
    IdentifierSymbolEntry *se = dynamic_cast<IdentifierSymbolEntry *>(id->getSymPtr());
    fprintf(stderr,"定义语句\n");
    if(se->isGlobal())
    {
        fprintf(stderr,"定义全局变量\n");
        Operand *addr;
        SymbolEntry *addr_se;
        addr_se = new IdentifierSymbolEntry(*se); //变量符号类型，加入符号表
        addr_se->setType(new PointerType(se->getType()));
        addr = new Operand(addr_se);
        se->setAddr(addr);

        //新加的：分配全局变量地址
        Instruction *alloca;
        if(expr!=nullptr){
            alloca=new globalAllocaInstruction(dst,se,expr->getOperand());
        }else{
            alloca=new globalAllocaInstruction(dst,se);
        }
        Unit *unit = builder->getUnit();
        unit->global_push(alloca); //中间代码部分处理全局变量：加全局变量指令
        mUnit.insertGlobal(se); //机器码部分处理全局变量：加到全局符号表

        //这部分不需要
        //if(expr!=nullptr){
            //expr->genCode();
            //Operand *ad = se->getAddr();
            //Operand *src = expr->getOperand();
            //Instruction *store = new StoreInstruction(ad, src); 
            //unit->global_push(store);
        //}
    }
    else if(se->isLocal())
    {
        fprintf(stderr,"定义局部变量\n");
        Function *func = builder->getInsertBB()->getParent();
        BasicBlock *entry = func->getEntry();
        Instruction *alloca;
        Operand *addr;
        SymbolEntry *addr_se;
        Type *type;
        type = new PointerType(se->getType()); //临时变量类型，加入符号表
        addr_se = new TemporarySymbolEntry(type, SymbolTable::getLabel());
        addr = new Operand(addr_se);
        alloca = new AllocaInstruction(addr, se); //声明指令                  // allocate space for local id in function stack.
        entry->insertFront(alloca);                                 // allocate instructions should be inserted into the begin of the entry block.
        se->setAddr(addr);                                          // set the addr operand in symbol entry so that we can use it in subsequent code generation.
        if(expr!=nullptr){
            expr->genCode();
            BasicBlock *bb = builder->getInsertBB();
            Operand *ad = se->getAddr();
            Operand *src = expr->getOperand();
            new StoreInstruction(ad, src, bb); 
        }
   
    }
}

void DefList::genCode()
{
    // Todo
    def1->genCode();
    def2->genCode();
}

void ConstDecl::genCode() //待实现：这里的指令和变量不太一样，instruction的output要加上const
{
    IdentifierSymbolEntry *se = dynamic_cast<IdentifierSymbolEntry *>(id->getSymPtr());
    if(se->isGlobal())
    {
        Operand *addr;
        SymbolEntry *addr_se;
        addr_se = new IdentifierSymbolEntry(*se);
        addr_se->setType(new PointerType(se->getType()));
        addr = new Operand(addr_se);
        se->setAddr(addr);

        //新加的：分配全局变量地址
        Instruction *alloca;
        if(expr!=nullptr){
            alloca=new globalAllocaInstruction(dst,se,expr->getOperand());
        }else{
            alloca=new globalAllocaInstruction(dst,se);
        }
        Unit *unit = builder->getUnit();
        unit->global_push(alloca);
        mUnit.insertGlobal(se);
    }
    else if(se->isLocal())
    {
        Function *func = builder->getInsertBB()->getParent();
        BasicBlock *entry = func->getEntry();
        Instruction *alloca;
        Operand *addr;
        SymbolEntry *addr_se;
        Type *type;
        type = new PointerType(se->getType());
        addr_se = new TemporarySymbolEntry(type, SymbolTable::getLabel());
        addr = new Operand(addr_se);
        alloca = new AllocaInstruction(addr, se);                   // allocate space for local id in function stack.
        entry->insertFront(alloca);                                 // allocate instructions should be inserted into the begin of the entry block.
        se->setAddr(addr);                                          // set the addr operand in symbol entry so that we can use it in subsequent code generation.
        if(expr!=nullptr){
            expr->genCode();
            BasicBlock *bb = builder->getInsertBB();
            Operand *ad = se->getAddr();
            Operand *src = expr->getOperand();
            new StoreInstruction(ad, src, bb); 
        }
   
    }
}

void WhileStmt::genCode()
{
    // Todo
    whileStack.push(this);

    Function *func;
    BasicBlock *cond_bb,*stmt_bb, *end_bb, *bb;

    fprintf(stderr,"WhileStmt\n");
    func = builder->getInsertBB()->getParent();
    cond_bb = new BasicBlock(func);
    stmt_bb= new BasicBlock(func);
    end_bb = new BasicBlock(func);

    bb = builder->getInsertBB();

    this->condBlock = cond_bb;
    this->endBlock = end_bb;

    new UncondBrInstruction(cond_bb,bb);

    builder->setInsertBB(cond_bb);
    genBr=1;//重点
    builder->setisCond(true);
    cond->genCode();
    builder->setisCond(false);
    backPatch(cond->trueList(), stmt_bb);
    backPatch(cond->falseList(), end_bb);

    builder->setInsertBB(stmt_bb);
    thenStmt->genCode();
    stmt_bb=builder->getInsertBB();
    new UncondBrInstruction(cond_bb, stmt_bb);

    builder->setInsertBB(end_bb); 

    whileStack.pop();
}

void BreakStmt::genCode()
{
    // Todo
    assert(whileStack.size()!=0);
    Function *func;
    BasicBlock *bb,*end_bb,*nextBlock;

    func = builder->getInsertBB()->getParent();
    bb = builder->getInsertBB();

    WhileStmt* whileStmt = whileStack.top();
    end_bb = whileStmt->getEndBlock();

    new UncondBrInstruction(end_bb, bb);
    nextBlock = new BasicBlock(func);
    builder->setInsertBB(nextBlock);
}
void ContinueStmt::genCode()
{
    // Todo
    assert(whileStack.size()!=0);
    Function* func;
    BasicBlock *bb, *cond_bb, *nextBlock;

    func = builder->getInsertBB()->getParent();
    bb = builder->getInsertBB();

    WhileStmt* whileStmt = whileStack.top();
    cond_bb = whileStmt->getCondBlock();

    new UncondBrInstruction(cond_bb, bb);
    nextBlock = new BasicBlock(func);
    builder->setInsertBB(nextBlock);
}
//新写的
void ReturnStmt::genCode()
{
    //有返回值就把返回语句插入当前块即可
    if(retValue!=NULL){ 
        retValue->genCode();
        Operand *src =retValue->getOperand();//获取返回值
        new RetInstruction(src, builder->getInsertBB());   
    }
    else{
        new RetInstruction(nullptr, builder->getInsertBB());
    }
}

void AssignStmt::genCode()
{
    BasicBlock *bb = builder->getInsertBB();
    expr->genCode();
    Operand *addr = dynamic_cast<IdentifierSymbolEntry*>(lval->getSymPtr())->getAddr();
    Operand *src = expr->getOperand();
    /***
     * We haven't implemented array yet, the lval can only be ID. So we just store the result of the `expr` to the addr of the id.
     * If you want to implement array, you have to caculate the address first and then store the result into it.
     */
    new StoreInstruction(addr, src, bb);
}

void Ast::typeCheck()
{
    if(root != nullptr)
        root->typeCheck();
}

void FunctionDef::typeCheck() 
{
    // Todo
    if(params!=NULL)
    {
        params->typeCheck();
    }
    // 获取函数的返回值类型
    returnType = ((FunctionType*)se->getType())->getRetType();
    // 判断函数体内是否有返回语句
    funcReturned = false;
    stmt->typeCheck();
    // 出错：函数需要返回值，但函数体里没返回值
    if(!funcReturned && !returnType->isVoid()){
        fprintf(stderr, "%s should return, but no returned value found\n", returnType->toStr().c_str());
        exit(EXIT_FAILURE);
    }
    // 如果void类型没写return需要补上
    //if(!funcReturned && returnType->isVoid()) {
        //this->voidAddRet = new ReturnStmt(nullptr);
    //}
    returnType = nullptr;
}

void FuncDefParamsNode::typeCheck()
{
    //不用检查
}

void FuncCallParamsNode::typeCheck()
{
    // 对每一个孩子节点进行常量计算
    for(ExprNode* param : paramsList) {
        param->typeCheck();
    }
}

void FuncCallNode::typeCheck()  //没进行类型转化
{   
    //类型转化成返回类型
    Type * tmp=symbolEntry->getType();
    FunctionType* temp=static_cast<FunctionType*>(tmp); 
    symbolEntry->setType(temp->getRetType());

    //函数应该传入的参数类型、个数
    std::vector<Type*> funcParamsType = (dynamic_cast<FunctionType*>(this->funcId->getSymPtr()->getType()))->getParamsType();
    // 调用时无参情况
    if(this->params==nullptr && funcParamsType.size() != 0){
        fprintf(stderr, "function %s call params number is not right\n",this->funcId->getSymPtr()->toStr().c_str());
        exit(EXIT_FAILURE);
    }
    else if(this->params==nullptr) { //实际和应该有的参数都是空
        return;
    }
    this->params->typeCheck(); 
    std::vector<ExprNode*> funcCallParams = this->params->getParamsList(); //调用时实际的参数类型、个数
    // 检查数量是否一致
    if(funcCallParams.size() != funcParamsType.size()) {
        fprintf(stderr, "function %s call params number is not right\n",this->funcId->getSymPtr()->toStr().c_str());
        exit(EXIT_FAILURE);
    }
    // 依此检查类型是否一致
    for(int i = 0; i < (int)funcParamsType.size(); i++){
        Type* needType = funcParamsType[i];
        Type* giveType = funcCallParams[i]->getSymPtr()->getType()->isFunc() ? 
        ((FunctionType*)funcCallParams[i]->getSymPtr()->getType())->getRetType() : funcCallParams[i]->getSymPtr()->getType();
        // 除了void类型都可以进行转化
        if((!needType->isVoid() && giveType->isVoid()) || (needType->isVoid() && !giveType->isVoid())){
            fprintf(stderr, "function %s call params type is not right\n",this->funcId->getSymPtr()->toStr().c_str());
            exit(EXIT_FAILURE);
        }
    }
}

void DefNode::typeCheck()
{
    // 由于目前DefNode用于函数定义时的参数列表，因此不用进行检查
}

void BinaryExpr::typeCheck()
{
    expr1->typeCheck();
    Type* type1 = expr1->getType();
    if(type1->isVoid()){  //检查是否void函数返回值参与运算
        fprintf(stderr, "type %s is void but in BinaryExpr\n", expr1->getType()->toStr().c_str());
        exit(EXIT_FAILURE);
    }
    expr2->typeCheck();
    Type* type2 = expr2->getType();
    if(type2->isVoid()){
        fprintf(stderr, "type %s is void but in BinaryExpr\n", expr2->getType()->toStr().c_str());
        exit(EXIT_FAILURE);
    }
    if(type1 != type2){ //包含int到bool的隐式转换，应该不会不等，int和bool表现出来都是int型
        if((type1->toStr()=="i32" && type2->toStr()=="i1")||(type1->toStr()=="i1" && type2->toStr()=="i32")){}
        else{
            fprintf(stderr, "type %s and %s mismatch",
            type1->toStr().c_str(), type2->toStr().c_str());
            exit(EXIT_FAILURE);
        }
    }
    symbolEntry->setType(type1);
    //如果两个操作数都是常数，可能需要调整BinaryExpr的type为常数类型
    //eg: symbolEntry->setType(type1);
}

void SingleExpr::typeCheck()
{
    // Todo
    expr->typeCheck();
    //检查是否void函数返回值参与运算
    Type* realType = expr->getType()->isFunc() ? ((FunctionType*)expr->getType())->getRetType() : expr->getType();
    if(realType->isVoid()){  //检查是否void函数返回值参与运算
        fprintf(stderr, "type %s is void but in SingleExpr\n", expr->getType()->toStr().c_str());
        exit(EXIT_FAILURE);
    }
    symbolEntry->setType(realType);
}

void Constant::typeCheck()
{
    // 由于还没添加常量类型，暂时不用检查
    //if(symbolEntry->getType()!=TypeSystem::constType)
    //{
        //fprintf(stderr, "常量类型没有设置为constant");
        //exit(EXIT_FAILURE);
        //return;
    //}
}

void Id::typeCheck()
{
    //如果不是数组的话，什么都不做
}

void IfStmt::typeCheck()
{
    // cond的int类型变成bool类型，不是int型报错
    cond->typeCheck();
    if(cond->getSymPtr()->getType()->isInt()==1){
        cond->getSymPtr()->setType(TypeSystem::boolType);
        //同时对then语句进行类型检查
        thenStmt->typeCheck();
    }
    else{
        if(cond->getSymPtr()->getType()->isFunc()==1)
        {
            if(((FunctionType*)cond->getType())->getRetType()->isInt()==1)
            {
                cond->getSymPtr()->setType(TypeSystem::boolType);
                thenStmt->typeCheck();
                return;
            }
        }
        fprintf(stderr,"if语句条件类型不正确\n");
        exit(EXIT_FAILURE);
    }
}

void IfElseStmt::typeCheck()
{
    // cond的int类型变成bool类型，不是int型报错
    cond->typeCheck();
    if(cond->getSymPtr()->getType()->isInt()==1){
        cond->getSymPtr()->setType(TypeSystem::boolType);
        //同时对then和else语句进行类型检查
        thenStmt->typeCheck();
        elseStmt->typeCheck();
    }
    else{
        if(cond->getSymPtr()->getType()->isFunc()==1)
        {
            if(((FunctionType*)cond->getType())->getRetType()->isInt()==1)
            {
                cond->getSymPtr()->setType(TypeSystem::boolType);
                thenStmt->typeCheck();
                elseStmt->typeCheck();
                return;
            }
        }
        fprintf(stderr,"if语句条件类型不正确\n");
        exit(EXIT_FAILURE);
    }
}

void CompoundStmt::typeCheck() //块语句的检查
{
    // Todo
    if(stmt!=nullptr){
        stmt->typeCheck();
    }
    //else {
        //stmt = new EmptyStmt();
    //}
}

void EmptyStmt::typeCheck()
{
    // 不需要检查
}

void ExprStmtNode::typeCheck()
{
    // Todo
     for(int i = 0; i<(int)exprList.size(); i++){
        exprList[i]->typeCheck();
    }
}

void SeqNode::typeCheck()
{
    // Todo
    stmt1->typeCheck();
     if(stmt2!=NULL){ stmt2->typeCheck();}
}

void DeclStmt::typeCheck()
{
    // Todo
    id->typeCheck();
    if (expr) {
        expr->typeCheck();
    }
}

void DefList::typeCheck()
{
    // Todo
    def1->typeCheck();
    def2->typeCheck();
}

void ConstDecl::typeCheck() //常量定义时的类型检查还需要完善
{
    // Todo
    id->typeCheck();
    if (expr) {
        expr->typeCheck();
    }
}

void WhileStmt::typeCheck()
{
    cond->typeCheck();
    //cond类型应该是int 型，转换成bool型
    if(cond->getSymPtr()->getType()->isInt()==1) {
        cond->getSymPtr()->setType(TypeSystem::boolType);
        if(thenStmt!=nullptr) { //循环体进行类型检查，inIteration是循环的层数
            inIteration++;
            thenStmt->typeCheck();
            inIteration--;
        }
    }
    else{
         if(cond->getSymPtr()->getType()->isFunc()==1)
        {
            if(((FunctionType*)cond->getType())->getRetType()->isInt()==1)
            {
                cond->getSymPtr()->setType(TypeSystem::boolType);
                if(thenStmt!=nullptr) {
                    inIteration++;
                    thenStmt->typeCheck();
                    inIteration--;
                }
                return;
            }
        }
        fprintf(stderr,"while语句条件类型不正确\n");
        exit(EXIT_FAILURE);
    }
}

void BreakStmt::typeCheck()
{
    // Todo
    if(!inIteration){
        fprintf(stderr, "break statement outside iterations\n");
        exit(EXIT_FAILURE);
    }
}
void ContinueStmt::typeCheck()
{
    // Todo
    if(!inIteration){
        fprintf(stderr, "continue statement outside iterations\n");
        exit(EXIT_FAILURE);
    }
}

void ReturnStmt::typeCheck()
{
    if(returnType == nullptr){//不是个函数
        fprintf(stderr, "This is not function,but return\n");
        exit(EXIT_FAILURE);
    }
    else if(returnType->isVoid() && retValue!=nullptr){//函数返回为void类型，但有return值
        fprintf(stderr, "value returned in a void() function\n");
        exit(EXIT_FAILURE);
    }
    else if(!returnType->isVoid() && retValue==nullptr){//需要return，但没return值
        fprintf(stderr, "%s should return, but no returned value found\n", returnType->toStr().c_str());
        exit(EXIT_FAILURE);
    }
    if(!returnType->isVoid()){ //return值的检查
        //retValue->typeCheck((Node**)&(retValue));
        retValue->typeCheck();
    }
    this->retType = returnType;
    funcReturned = true;
}

void AssignStmt::typeCheck()
{
    lval->typeCheck();
    expr->typeCheck();
    //if(lval->getType()->isConst()) {
        //fprintf(stderr, "Unable to assign value to const variable %s\n", lval->getSymPtr()->toStr().c_str());
        //exit(EXIT_FAILURE);
    //}
    if(expr->getType()->isFunc() && ((FunctionType*)(expr->getType()))->getRetType()->isVoid()){//返回值为void的函数做运算数
        fprintf(stderr, "type %s is void but in AssignStmt\n", expr->getType()->toStr().c_str());
        exit(EXIT_FAILURE);
    }
}

void BinaryExpr::output(int level)
{
    std::string op_str;
    switch(op)
    {
        case ADD:
            op_str = "add";
            break;
        case SUB:
            op_str = "sub";
            break;
        case MUL:
            op_str = "mul";
            break;
        case DIV:
            op_str = "div";
            break;
        case MOD:
            op_str = "mod";
            break;

        case AND:
            op_str = "and";
            break;
        case OR:
            op_str = "or";
            break;
            
        case LESS:
            op_str = "less";
            break;
        case LESSEQUAL:
            op_str = "lessequal";
            break;
        case GREATER:
            op_str = "greater";
            break;
        case GREATEREQUAL:
            op_str = "greaterequal";
            break;            
        case EQUAL:
            op_str = "equal";
            break;   
        case EXCLAIMEQUAL:
            op_str = "exclaimequal";
            break;                     
    }
    fprintf(yyout, "%*cBinaryExpr\top: %s\n", level, ' ', op_str.c_str());
    expr1->output(level + 4);
    expr2->output(level + 4);
}

void Ast::output()
{
    fprintf(yyout, "program\n");
    if(root != nullptr)
        root->output(4);
}

void SingleExpr::output(int level)
{
    std::string op_str;
    switch(op)
    {
        case SUB:
            op_str = "minus";
            break;
        case EXCLAIM:
            op_str = "exclaim";
            break;
                   
    }
    fprintf(yyout, "%*cSingleExpr\top: %s\n", level, ' ', op_str.c_str());
    expr->output(level + 4);
}

void Constant::output(int level)
{
    std::string type, value;
    type = symbolEntry->getType()->toStr();
    value = symbolEntry->toStr();
    fprintf(yyout, "%*cIntegerLiteral\tvalue: %s\ttype: %s\n", level, ' ',
            value.c_str(), type.c_str());
}

void Id::output(int level)
{
    std::string name, type;
    int scope;
    name = symbolEntry->toStr();
    type = symbolEntry->getType()->toStr();
    scope = dynamic_cast<IdentifierSymbolEntry*>(symbolEntry)->getScope();
    fprintf(yyout, "%*cId\tname: %s\tscope: %d\ttype: %s\n", level, ' ',
            name.c_str(), scope, type.c_str());
}

void CompoundStmt::output(int level)
{
    fprintf(yyout, "%*cCompoundStmt\n", level, ' ');
    if(stmt == nullptr){
        fprintf(yyout, "%*cNull Stmt\n", level+4, ' ');
    }
    else{
        stmt->output(level + 4);
    }
}

void EmptyStmt::output(int level)
{
    fprintf(yyout, "%*cEmptyStmt\n", level, ' ');
}

void ExprStmtNode::addNext(ExprNode* next)
{
    exprList.push_back(next);
}

void ExprStmtNode::output(int level)
{
    fprintf(yyout, "%*cExprStmtNode\n", level, ' ');
    for(auto expr : exprList)
    {
        expr->output(level+4);
    }
}

void SeqNode::output(int level)
{
    fprintf(yyout, "%*cSequence\n", level, ' ');
    stmt1->output(level);
    stmt2->output(level);
}

void DeclStmt::output(int level)
{
    fprintf(yyout, "%*cDeclStmt\n", level, ' ');
    id->output(level + 4);
    if(expr){
        expr->output(level + 4);
    }
}

void ConstDecl::output(int level)
{
    fprintf(yyout, "%*cConstDecl\n", level, ' ');
    id->output(level + 4);
    if(expr){
        expr->output(level + 4);
    }
}

void DefList::output(int level)
{
    fprintf(yyout, "%*cDefList\n", level, ' ');
    def1->output(level+4);
    def2->output(level+4);
}

void DefNode::output(int level)
{
    std::string constStr = isConst ? "true" : "false";
    std::string arrayStr = isArray ? "true" : "false";
    fprintf(yyout, "%*cDefNode\tisConst:%s\tisArray:%s\n", level, ' ', constStr.c_str(), arrayStr.c_str());
    id->output(level+4);
    if(initVal == nullptr){
        fprintf(yyout, "%*cnull\n", level+4, ' ');
    }
    else{
        initVal->output(level+4);
    }
}

void IfStmt::output(int level)
{
    fprintf(yyout, "%*cIfStmt\n", level, ' ');
    cond->output(level + 4);
    thenStmt->output(level + 4);
}

void IfElseStmt::output(int level)
{
    fprintf(yyout, "%*cIfElseStmt\n", level, ' ');
    cond->output(level + 4);
    thenStmt->output(level + 4);
    elseStmt->output(level + 4);
}

void WhileStmt::output(int level)
{
    fprintf(yyout, "%*cWhileStmt\n", level, ' ');
    cond->output(level + 4);
    thenStmt->output(level + 4);
}

void BreakStmt::output(int level)
{
    fprintf(yyout, "%*cBreakStmt\n", level, ' ');
}

void ContinueStmt::output(int level)
{
    fprintf(yyout, "%*cContinueStmt\n", level, ' ');
}

void ReturnStmt::output(int level)
{
    fprintf(yyout, "%*cReturnStmt\n", level, ' ');
    retValue->output(level + 4);
}

void AssignStmt::output(int level)
{
    fprintf(yyout, "%*cAssignStmt\n", level, ' ');
    lval->output(level + 4);
    expr->output(level + 4);
}

void FuncDefParamsNode::addNext(Id* next)
{
    paramsList.push_back(next);
}

std::vector<Type*> FuncDefParamsNode::getParamsType()
{
    std::vector<Type*> typeArray;
    for(auto param : paramsList){
        typeArray.push_back(param->getType());
    }
    return typeArray;
}

void FuncDefParamsNode::output(int level)
{
    fprintf(yyout, "%*cFuncDefParamsNode\n", level, ' ');
    for(auto param : paramsList){
        param->output(level+4);
    }
}

void FunctionDef::output(int level)
{
    std::string name, type;
    name = se->toStr();
    type = se->getType()->toStr();
    fprintf(yyout, "%*cFunctionDefine function name: %s\ttype: %s\n", level, ' ', 
            name.c_str(), type.c_str());
    if(params!=nullptr){
        params->output(level+4);
    }
    else{
        fprintf(yyout, "%*cFuncDefParamsNode NULL\n", level+4, ' ');
    }
    stmt->output(level + 4);
}

void FuncCallNode::output(int level)
{
    std::string name, type;
    int scope;
    SymbolEntry* funcEntry = funcId->getSymbolEntry();
    name = funcEntry->toStr();
    type = funcEntry->getType()->toStr();
    scope = dynamic_cast<IdentifierSymbolEntry*>(funcEntry)->getScope();
    fprintf(yyout, "%*cFuncCallNode\tfuncName: %s\t funcType: %s\tscope: %d\n", 
            level, ' ', name.c_str(), type.c_str(), scope);
    if(params!=nullptr){
        params->output(level+4);
    }
    else{
        fprintf(yyout, "%*cFuncCallParamsNode NULL\n", level+4, ' ');
    }
}

void FuncCallParamsNode::addNext(ExprNode* next)
{
    paramsList.push_back(next);
}

void FuncCallParamsNode::output(int level)
{
    fprintf(yyout, "%*cFuncCallParamsNode\n", level, ' ');
    for(auto param : paramsList){
        param->output(level+4);
    }
}

int BinaryExpr::getValue() {
    int value = 0;
    switch (op) {
        case ADD:
            value = expr1->getValue() + expr2->getValue();
            break;
        case SUB:
            value = expr1->getValue() - expr2->getValue();
            break;
        case MUL:
            value = expr1->getValue() * expr2->getValue();
            break;
        case DIV:
            if(expr2->getValue())
                value = expr1->getValue() / expr2->getValue();
            break;
        case MOD:
            value = expr1->getValue() % expr2->getValue();
            break;
        case AND:
            value = expr1->getValue() && expr2->getValue();
            break;
        case OR:
            value = expr1->getValue() || expr2->getValue();
            break;
        case LESS:
            value = expr1->getValue() < expr2->getValue();
            break;
        case LESSEQUAL:
            value = expr1->getValue() <= expr2->getValue();
            break;
        case GREATER:
            value = expr1->getValue() > expr2->getValue();
            break;
        case GREATEREQUAL:
            value = expr1->getValue() >= expr2->getValue();
            break;
        case EQUAL:
            value = expr1->getValue() == expr2->getValue();
            break;
        case EXCLAIMEQUAL:
            value = expr1->getValue() != expr2->getValue();
            break;
    }
    return value;
}

int SingleExpr::getValue() {
    int value = 0;
    switch (op) {
        case EXCLAIM:
            value = !(expr->getValue());
            break;
        case SUB:
            value = -(expr->getValue());
            break;
    }
    return value;
}

int Constant::getValue() {
    // assert(symbolEntry->getType()->isInt());
    return ((ConstantSymbolEntry*)symbolEntry)->getValue();
}

int Id::getValue() {
    // assert(symbolEntry->getType()->isInt());
    return ((IdentifierSymbolEntry*)symbolEntry)->getValue();
}
