//===--- Parameter.h - Functions & closures parameters ----------*- C++ -*-===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2015 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See http://swift.org/LICENSE.txt for license information
// See http://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//
//
// This file defines the Parameter class, the ParameterList class and support
// logic.
//
//===----------------------------------------------------------------------===//

#ifndef SWIFT_AST_PARAMETER_H
#define SWIFT_AST_PARAMETER_H

#include "swift/AST/DefaultArgumentKind.h"
#include "swift/AST/TypeLoc.h"
#include "swift/AST/Decl.h"
#include "swift/Basic/OptionSet.h"

namespace swift {
  class ParamDecl;
  class ExprHandle;
  
/// This describes a single parameter, including such feats as:
///
///   a b : Int           //< Differing internal vs external name.
///   inout a : Int       //< inout parameter.
///   @autoclosure a : T  //< Parameter attribute.
///   a : Int = 42        //< Default value.
///   a : Int...          //< Varargs parameter.
///
/// A parameter is stored as ParamDecls with additional information in the
/// Parameter struct to represent source information as well as extra semantics.
///
struct Parameter {
  /// The decl keeps track of the internal and external parameter name, as well
  /// as the parameter attributes.
  ParamDecl *decl;

  /// This is the location of the ":" token.
  SourceLoc colonLoc;
  
  /// This is the type specified, including location information.
  TypeLoc type;
  
  // TODO: ExprHandle can probably go away now, we should be able to just have
  // an Expr* here, along with a "is checked" bit.

  /// The default value, if any, along with whether this is varargs.
  llvm::PointerIntPair<ExprHandle *, 1, bool> defaultValueAndIsVariadic;

  void setDefaultValue(ExprHandle *H) {
    defaultValueAndIsVariadic.setPointer(H);
  }
  ExprHandle *getDefaultValue() const {
    return defaultValueAndIsVariadic.getPointer();
  }
  /// Whether or not this parameter is varargs.
  bool isVariadic() const { return defaultValueAndIsVariadic.getInt(); }
  void setVariadic(bool value = true) {defaultValueAndIsVariadic.setInt(value);}
  
  /// Information about a symbolic default argument, like __FILE__.
  DefaultArgumentKind defaultArgumentKind;
  
  /// Remove the type of this varargs element designator, without the array
  /// type wrapping it.  A parameter like "Int..." will have formal parameter
  /// type of "[Int]" and this returns "Int".
  static Type getVarargBaseTy(Type VarArgT);
  
  /// Remove the type of this varargs element designator, without the array
  /// type wrapping it.
  Type getVarargBaseTy() const {
    assert(isVariadic());
    return getVarargBaseTy(decl->getType());
  }
  
  /// Create a simple parameter without location information.
  static Parameter withoutLoc(ParamDecl *decl) {
    Parameter result;
    result.decl = decl;
    result.type = TypeLoc::withoutLoc(decl->getType());
    return result;
  }
  
  /// Return the full source range of this parameter.
  SourceRange getSourceRange() const;
  SourceLoc getStartLoc() const { return getSourceRange().Start; }
  SourceLoc getEndLoc() const { return getSourceRange().End; }

  void dump() const;
  void dump(raw_ostream &OS, unsigned Indent = 0) const;
  
  //  void print(raw_ostream &OS) const;
};


/// This describes a list of parameters.  Each parameter descriptor is tail
/// allocated onto this list.
class alignas(alignof(Parameter)) ParameterList {
  void *operator new(size_t Bytes) throw() = delete;
  void operator delete(void *Data) throw() = delete;
  void *operator new(size_t Bytes, void *Mem) throw() = delete;
  void *operator new(size_t Bytes, ASTContext &C,
                     unsigned Alignment = 8);

  SourceLoc LParenLoc, RParenLoc;
  size_t numParameters;

  ParameterList(size_t numParameters) : numParameters(numParameters) {}
  void operator=(const ParameterList&) = delete;
public:
  /// Create a parameter list with the specified parameters.
  static ParameterList *create(const ASTContext &C, SourceLoc LParenLoc,
                               ArrayRef<Parameter> params,
                               SourceLoc RParenLoc);

  /// Create a parameter list with the specified parameters, with no location
  /// info for the parens.
  static ParameterList *create(const ASTContext &C,
                               ArrayRef<Parameter> params) {
    return create(C, SourceLoc(), params, SourceLoc());
  }
 
  /// Create an empty parameter list.
  static ParameterList *createEmpty(const ASTContext &C,
                                    SourceLoc LParenLoc = SourceLoc(),
                                    SourceLoc RParenLoc = SourceLoc()) {
    return create(C, LParenLoc, {}, RParenLoc);
  }
  
  /// Create a parameter list for a single parameter lacking location info.
  static ParameterList *createWithoutLoc(ParamDecl *decl) {
    return create(decl->getASTContext(), Parameter::withoutLoc(decl));
  }
  
  /// Create an implicit 'self' decl for a method in the specified decl context.
  /// If 'static' is true, then this is self for a static method in the type.
  ///
  /// Note that this decl is created, but it is returned with an incorrect
  /// DeclContext that needs to be set correctly.  This is automatically handled
  /// when a function is created with this as part of its argument list.
  ///
  static ParameterList *createSelf(SourceLoc loc, DeclContext *DC,
                                   bool isStaticMethod = false,
                                   bool isInOut = false);

  SourceLoc getLParenLoc() const { return LParenLoc; }
  SourceLoc getRParenLoc() const { return RParenLoc; }
  
  size_t getNumParameters() const {
    return numParameters;
  }
  
  MutableArrayRef<Parameter> getParameters() {
    auto Ptr = reinterpret_cast<Parameter*>(this + 1);
    return { Ptr, numParameters };
  }
  ArrayRef<Parameter> getParameters() const {
    auto Ptr = reinterpret_cast<const Parameter*>(this + 1);
    return { Ptr, numParameters };
  }
  
  const Parameter &getParameter(unsigned i) const {
    return getParameters()[i];
  }

  Parameter &getParameter(unsigned i) {
    return getParameters()[i];
  }
  
  /// Change the DeclContext of any contained parameters to the specified
  /// DeclContext.
  void setDeclContextOfParamDecls(DeclContext *DC);
  
  
  /// Flags used to indicate how ParameterList cloning should operate.
  enum CloneFlags {
    /// The cloned ParamDecls should be marked implicit.
    Implicit = 0x01,
    /// The cloned pattern is for an inherited constructor; mark default
    /// arguments as inherited, and mark unnamed arguments as named.
    Inherited = 0x02
  };
  
  /// Make a duplicate copy of this parameter list.  This allocates copies of
  /// the ParamDecls, so they can be reparented into a new DeclContext.
  ParameterList *clone(const ASTContext &C,
                       OptionSet<CloneFlags> options = None) const;
  
  /// Return a TupleType or ParenType for this parameter list.  This returns a
  /// null type if one of the ParamDecls does not have a type set for it yet.
  Type getType(const ASTContext &C) const;
  
  /// Return the full function type for a set of curried parameter lists that
  /// returns the specified result type.  This returns a null type if one of the
  /// ParamDecls does not have a type set for it yet.
  ///
  static Type getFullType(Type resultType, ArrayRef<ParameterList*> PL);
  
  
  /// Return the full source range of this parameter.
  SourceRange getSourceRange() const;
  SourceLoc getStartLoc() const { return getSourceRange().Start; }
  SourceLoc getEndLoc() const { return getSourceRange().End; }

  void dump() const;
  void dump(raw_ostream &OS, unsigned Indent = 0) const;
  
  //  void print(raw_ostream &OS) const;
};

} // end namespace swift.

#endif
