/* Copyright (c) 2008-2012, Avian Contributors

   Permission to use, copy, modify, and/or distribute this software
   for any purpose with or without fee is hereby granted, provided
   that the above copyright notice and this permission notice appear
   in all copies.

   There is NO WARRANTY for this software.  See license.txt for
   details. */

#ifndef AVIAN_CODEGEN_COMPILER_SITE_H
#define AVIAN_CODEGEN_COMPILER_SITE_H

namespace avian {
namespace codegen {
namespace compiler {

class Context;

const unsigned RegisterCopyCost = 1;
const unsigned AddressCopyCost = 2;
const unsigned ConstantCopyCost = 3;
const unsigned MemoryCopyCost = 4;
const unsigned CopyPenalty = 10;

class SiteMask {
 public:
  SiteMask(): typeMask(~0), registerMask(~0), frameIndex(AnyFrameIndex) { }

  SiteMask(uint8_t typeMask, uint32_t registerMask, int frameIndex):
    typeMask(typeMask), registerMask(registerMask), frameIndex(frameIndex)
  { }

  uint8_t typeMask;
  uint32_t registerMask;
  int frameIndex;
};

class Site {
 public:
  Site(): next(0) { }
  
  virtual Site* readTarget(Context*, Read*) { return this; }

  virtual unsigned toString(Context*, char*, unsigned) = 0;

  virtual unsigned copyCost(Context*, Site*) = 0;

  virtual bool match(Context*, const SiteMask&) = 0;

  virtual bool loneMatch(Context*, const SiteMask&) = 0;

  virtual bool matchNextWord(Context*, Site*, unsigned) = 0;
  
  virtual void acquire(Context*, Value*) { }

  virtual void release(Context*, Value*) { }

  virtual void freeze(Context*, Value*) { }

  virtual void thaw(Context*, Value*) { }

  virtual bool frozen(Context*) { return false; }

  virtual lir::OperandType type(Context*) = 0;

  virtual void asAssemblerOperand(Context*, Site*, lir::Operand*) = 0;

  virtual Site* copy(Context*) = 0;

  virtual Site* copyLow(Context*) = 0;

  virtual Site* copyHigh(Context*) = 0;

  virtual Site* makeNextWord(Context*, unsigned) = 0;

  virtual SiteMask mask(Context*) = 0;

  virtual SiteMask nextWordMask(Context*, unsigned) = 0;

  virtual unsigned registerSize(Context*) { return vm::TargetBytesPerWord; }

  virtual unsigned registerMask(Context*) { return 0; }

  virtual bool isVolatile(Context*) { return false; }

  Site* next;
};

class SiteIterator {
 public:
  SiteIterator(Context* c, Value* v, bool includeBuddies = true,
               bool includeNextWord = true);

  Site** findNext(Site** p);
  bool hasMore();
  Site* next();
  void remove(Context* c);

  Context* c;
  Value* originalValue;
  Value* currentValue;
  bool includeBuddies;
  bool includeNextWord;
  uint8_t pass;
  Site** next_;
  Site** previous;
};

Site* constantSite(Context* c, Promise* value);
Site* constantSite(Context* c, int64_t value);

Promise* combinedPromise(Context* c, Promise* low, Promise* high);
Promise* shiftMaskPromise(Context* c, Promise* base, unsigned shift, int64_t mask);

class ConstantSite: public Site {
 public:
  ConstantSite(Promise* value): value(value) { }

  virtual unsigned toString(Context*, char* buffer, unsigned bufferSize) {
    if (value->resolved()) {
      return vm::snprintf
        (buffer, bufferSize, "constant %" LLD, value->value());
    } else {
      return vm::snprintf(buffer, bufferSize, "constant unresolved");
    }
  }

  virtual unsigned copyCost(Context*, Site* s) {
    return (s == this ? 0 : ConstantCopyCost);
  }

  virtual bool match(Context*, const SiteMask& mask) {
    return mask.typeMask & (1 << lir::ConstantOperand);
  }

  virtual bool loneMatch(Context*, const SiteMask&) {
    return true;
  }

  virtual bool matchNextWord(Context* c, Site* s, unsigned) {
    return s->type(c) == lir::ConstantOperand;
  }

  virtual lir::OperandType type(Context*) {
    return lir::ConstantOperand;
  }

  virtual void asAssemblerOperand(Context* c, Site* high,
                                  lir::Operand* result)
  {
    Promise* v = value;
    if (high != this) {
      v = combinedPromise(c, value, static_cast<ConstantSite*>(high)->value);
    }
    new (result) lir::Constant(v);
  }

  virtual Site* copy(Context* c) {
    return constantSite(c, value);
  }

  virtual Site* copyLow(Context* c) {
    return constantSite(c, shiftMaskPromise(c, value, 0, 0xFFFFFFFF));
  }

  virtual Site* copyHigh(Context* c) {
    return constantSite(c, shiftMaskPromise(c, value, 32, 0xFFFFFFFF));
  }

  virtual Site* makeNextWord(Context* c, unsigned) {
    abort(c);
  }

  virtual SiteMask mask(Context*) {
    return SiteMask(1 << lir::ConstantOperand, 0, NoFrameIndex);
  }

  virtual SiteMask nextWordMask(Context*, unsigned) {
    return SiteMask(1 << lir::ConstantOperand, 0, NoFrameIndex);
  }

  Promise* value;
};

} // namespace compiler
} // namespace codegen
} // namespace avian

#endif // AVIAN_CODEGEN_COMPILER_SITE_H
