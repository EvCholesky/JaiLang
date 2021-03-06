/* Copyright (C) 2015 Evan Christensen
|
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated 
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation the 
| rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit 
| persons to whom the Software is furnished to do so, subject to the following conditions:
| 
| The above copyright notice and this permission notice shall be included in all copies or substantial portions of the 
| Software.
| 
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE 
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR 
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#include "LlvmcDIBuilder.h"

#ifdef _WINDOWS
#pragma warning ( push )
#pragma warning(disable : 4141)
#pragma warning(disable : 4244)
#pragma warning(disable : 4267)
#pragma warning(disable : 4291)
#pragma warning(disable : 4624)
#pragma warning(disable : 4800)
#pragma warning(disable : 4996)
#endif
#include "llvm-c/Core.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/DIBuilder.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Module.h"
#include "llvm/Target/TargetMachine.h"
#ifdef _WINDOWS
#pragma warning ( push )
#endif

using namespace llvm;



inline LLVMValueRef wrap(MDNode * pMdnode) 
{
	return wrap(MetadataAsValue::get(pMdnode->getContext(), pMdnode));
}

static MDNode * PMdnodeExtract(MetadataAsValue * pMav) 
{
	if (!pMav)
		return nullptr;

	Metadata * pMetadata = pMav->getMetadata();
	assert((isa<MDNode>(pMetadata) || isa<ConstantAsMetadata>(pMetadata)) && "metadata node or a constant expected");

	MDNode * pMdnode = dyn_cast<MDNode>(pMetadata);
	if (pMdnode)
		return pMdnode;

	return MDNode::get(pMav->getContext(), pMetadata);
}

inline MDNode * PMdnodeExtract(LLVMValueRef pNode)
{
	if (!pNode)
		return nullptr;

	MetadataAsValue * pMav = unwrap<MetadataAsValue>(pNode);
	return PMdnodeExtract(pMav);
}

inline Metadata ** PPMetadataUnwrap(LLVMValueRef * ppLval, unsigned cLval)
{
	Metadata ** apMetadata = reinterpret_cast<Metadata **>(ppLval);

	LLVMValueRef * ppLvalEnd = ppLval + cLval;
	Metadata ** ppMetadata = apMetadata;
	for (LLVMValueRef * ppLvalIt = ppLval; ppLvalIt != ppLvalEnd; ++ppLvalIt, ++ppMetadata)
	{
		MetadataAsValue * pMav = unwrap<MetadataAsValue>(*ppLvalIt);
		(*ppMetadata) = cast<Metadata>(PMdnodeExtract(pMav));
	}

	return apMetadata;
}

static llvm::TargetMachine *unwrap(LLVMOpaqueTargetMachine * pLtmachine) 
{
  return reinterpret_cast<llvm::TargetMachine *>(pLtmachine);
}

void SetUseFastIsel(LLVMOpaqueTargetMachine * pLtmachine)
{
	unwrap(pLtmachine)->setFastISel(true);
	unwrap(pLtmachine)->setO0WantsFastISel(true);
}

LLVMDIBuilderRef LLVMCreateDIBuilder(LLVMModuleRef pMod)
{
	Module * pModule = unwrap(pMod);
	pModule->addModuleFlag(Module::Warning, "Debug Info Version", llvm::DEBUG_METADATA_VERSION);	
	pModule->addModuleFlag(Module::Warning, "CodeView", 1);	

	return wrap(new DIBuilder(*pModule));
}

void LLVMDisposeDIBuilder(LLVMDIBuilderRef pDib)
{ 
	delete unwrap(pDib); 
}

void LLVMDIBuilderFinalize(LLVMDIBuilderRef pDib)
{
	unwrap(pDib)->finalize();
}


LLVMValueRef LLVMGlobalStringPtr(LLVMBuilderRef pLbuild, LLVMModuleRef pMod, const char * pChzString, const char * pChzName)
{
	// replacing LLVMGlobalStringPTr as it seems to crash when outside of a BB because it tries to look up the module from a null.

	LLVMContext &context = unwrap(pLbuild)->getContext();
	Constant * strConstant = ConstantDataArray::getString(context, pChzString);
	Module * pModule = unwrap(pMod);
	unsigned AddressSpace = 0;
	GlobalVariable * pLglob = new GlobalVariable(*pModule, strConstant->getType(),
                                          true, GlobalValue::PrivateLinkage,
                                          strConstant, pChzName, nullptr,
                                          GlobalVariable::NotThreadLocal,
                                          AddressSpace);

	pLglob->setUnnamedAddr(GlobalValue::UnnamedAddr::Global);

	LLVMOpaqueValue * apLvalIndex[2] = {};
	apLvalIndex[0] = LLVMConstInt(LLVMInt32Type(), 0, false);
	apLvalIndex[1] = apLvalIndex[0];

	return LLVMBuildInBoundsGEP(pLbuild, wrap(pLglob),
                                  apLvalIndex, 2,
                                  pChzName);
}

LLVMValueRef LLVMDIBuilderCreateCompileUnit(
		LLVMDIBuilderRef pDib, 
		unsigned nLanguage,
		LLVMOpaqueValue * pLvalFile,
		const char * pChzProducer,
		LLVMBool fIsOptimized,
		const char * pChzFlags,
		unsigned nRuntimeVersion)
{
	StringRef strrProducer(pChzProducer);
	StringRef strrFlags(pChzFlags);
	DIFile * pDifile = cast<DIFile>(PMdnodeExtract(pLvalFile));

	DICompileUnit * pCU = unwrap(pDib)->createCompileUnit(
											nLanguage,
											pDifile,
											strrProducer,
											fIsOptimized,
											strrFlags,
											nRuntimeVersion);

	LLVMValueRef pLvalCU = wrap(pCU);
	assert(pCU == cast<DIScope>(PMdnodeExtract(pLvalCU)));
	return pLvalCU;
}

LLVMValueRef LLVMDIBuilderCreateFile(LLVMDIBuilderRef pDib, const char * pChzFilename, const char * pChzDirectory)
{
	StringRef strrFilename(pChzFilename);
	StringRef strrDirectory(pChzDirectory);
	DIFile * pFile = unwrap(pDib)->createFile(strrFilename, strrDirectory);
	return wrap(pFile);
}

LLVMValueRef LLVMCreateDebugLocation(LLVMBuilderRef pLbuild, int nLine, int nCol, LLVMValueRef pLvalScope)
{
	MDNode * pMdnodeScope = PMdnodeExtract(unwrap<MetadataAsValue>(pLvalScope));
	DebugLoc debugloc = DebugLoc::get(nLine, nCol, pMdnodeScope);

	LLVMContext &context = unwrap(pLbuild)->getContext();
	return wrap(MetadataAsValue::get(context, debugloc.getAsMDNode()));
}

/*
void LLVMBuilderSetCurrentDebugLocation(LLVMBuilderRef pLbuild, int nRow, int nCol, LLVMValueRef pLvalScope)
{
	MetadataAsValue * pMav = unwrap<MetadataAsValue>(pLvalScope);
	unwrap(pLbuild)->SetCurrentDebugLocation(DebugLoc::get(nRow, nCol, PMdnodeExtract(pMav))); 
}

void LLVMBuilderClearCurrentDebugLocation(LLVMBuilderRef pLbuild)
{
	unwrap(pLbuild)->SetCurrentDebugLocation(DebugLoc());
}
*/



// Types

LLVMValueRef LLVMDIBuilderCreateEnumerator(LLVMDIBuilderRef pDib, const char * pChzName, int64_t nValue)
{
	StringRef strrName(pChzName);
	return wrap(unwrap(pDib)->createEnumerator(strrName, nValue));
}

LLVMValueRef LLVMDIBuilderCreateNullPtr(LLVMDIBuilderRef pDib)
{
	return wrap(unwrap(pDib)->createNullPtrType());
}

LLVMValueRef LLVMDIBuilderCreateBasicType(
				LLVMDIBuilderRef pDib, 
				const char * pChzName,
			    uint64_t cBitSize,
			    unsigned nDwarfEncoding) 
{
	StringRef strrName(pChzName);
	return wrap(unwrap(pDib)->createBasicType(strrName, cBitSize, nDwarfEncoding));
}

LLVMValueRef LLVMDIBuilderCreateQualifiedType(LLVMDIBuilderRef pDib, unsigned nDwarfTag, LLVMValueRef pLvalFromType) 
{
	DIType * pDitype = cast<DIType>(PMdnodeExtract(pLvalFromType));
	return wrap(unwrap(pDib)->createQualifiedType(nDwarfTag, pDitype));
}

LLVMValueRef LLVMDIBuilderCreatePointerType(
	LLVMDIBuilderRef pDib,
    LLVMValueRef pLvalPointeeType,
    uint64_t cBitSize,
    uint64_t cBitAlign, 
	const char * pChzName) 
{
	StringRef strrName(pChzName);
	DIType * pDitypePointee = cast<DIType>(PMdnodeExtract(pLvalPointeeType));
	return wrap(unwrap(pDib)->createPointerType(pDitypePointee, cBitSize, cBitAlign, None, strrName));
}

LLVMValueRef LLVMDIBuilderCreateTypeDef(
	LLVMDIBuilderRef pDib,
    LLVMValueRef pLvalOriginalType,
	const char * pChzTypedefName,
    LLVMValueRef pLvalFile,
	unsigned nLine,
    LLVMValueRef pLvalScope)
{
	StringRef strrTypedefName(pChzTypedefName);
	DIType * pDitypeOriginal = cast<DIType>(PMdnodeExtract(pLvalOriginalType));
	DIFile * pDifile = cast<DIFile>(PMdnodeExtract(pLvalFile));
	DIScope * pDiscope = cast<DIScope>(PMdnodeExtract(pLvalScope));

	return wrap(unwrap(pDib)->createTypedef(pDitypeOriginal, strrTypedefName, pDifile, nLine, pDiscope));
}

LLVMValueRef LLVMDIBuilderCreateMemberType(
    LLVMDIBuilderRef pDib,
	LLVMValueRef pLvalScope,
	const char * pChzName,
	LLVMValueRef pLvalFile,
    unsigned nLine,
	uint64_t cBitSize,
	uint64_t cBitAlign,
    uint64_t dBitOffset,
	unsigned nFlags,
	LLVMValueRef pLvalParentType) 
{
	StringRef strrName(pChzName);
	DIScope * pDiscope = cast<DIScope>(PMdnodeExtract(pLvalScope));
	DIFile * pDifile = cast<DIFile>(PMdnodeExtract(pLvalFile));
	DIType * pDitypeParent = cast<DIType>(PMdnodeExtract(pLvalParentType));
	
	return wrap(unwrap(pDib)->createMemberType(pDiscope, strrName, pDifile, nLine, cBitSize, cBitAlign, dBitOffset, (DINode::DIFlags)nFlags, pDitypeParent));
}

LLVMValueRef LLVMDIBuilderCreateClassType(
    LLVMDIBuilderRef pDib, 
	LLVMValueRef pLvalScope,
	const char * pChzName,
	LLVMValueRef pLvalFile,
    unsigned nLine, 
	uint64_t cBitSize, 
	uint64_t cBitAlign,
    uint64_t dBitOffset,
	unsigned nFlags, 
	LLVMValueRef pLvalDerivedFrom,
    LLVMValueRef * ppLvalElements, 
	unsigned cElement,
	LLVMValueRef pLvalVTableHolder,
    LLVMValueRef pLvalTemplateParms) 
{
	StringRef strrName(pChzName);
	DIScope * pDiscope = cast<DIScope>(PMdnodeExtract(pLvalScope));
	DIFile * pDifile = cast<DIFile>(PMdnodeExtract(pLvalFile));
	DIType * pDitypeDerivedFrom = cast_or_null<DIType>(PMdnodeExtract(pLvalDerivedFrom));
	DIType * pDitypeVTableHolder = cast_or_null<DIType>(PMdnodeExtract(pLvalVTableHolder));
	MDNode * pMdnodeTemplateParms = PMdnodeExtract(pLvalTemplateParms);

	auto pDibuild = unwrap(pDib);
	ArrayRef<Metadata *> ary(PPMetadataUnwrap(ppLvalElements, cElement), cElement);
	DINodeArray diaryElements = pDibuild->getOrCreateArray(ary);

	return wrap(pDibuild->createClassType(
							pDiscope,
							strrName,
							pDifile,
							nLine,
							cBitSize,
							cBitAlign,
							dBitOffset,
							(DINode::DIFlags)nFlags,
							pDitypeDerivedFrom,
							diaryElements,
							pDitypeVTableHolder,
							pMdnodeTemplateParms));
}

LLVMValueRef LLVMDIBuilderCreateStructType(
    LLVMDIBuilderRef pDib, 
	LLVMValueRef pLvalScope, 
	const char * pChzName, 
	LLVMValueRef pLvalFile,
    unsigned nLine, 
	uint64_t cBitSize, 
	uint64_t cBitAlign, 
	unsigned nFlags,
	LLVMValueRef pLvalDerivedFrom,
    LLVMValueRef * ppLvalElements, 
	unsigned cElement,
	LLVMValueRef pLvalVTableHolder,
	unsigned nRuntimeLanguage) 
{
	StringRef strrName(pChzName);
	DIScope * pDiscope = cast<DIScope>(PMdnodeExtract(pLvalScope));
	DIFile * pDifile = cast<DIFile>(PMdnodeExtract(pLvalFile));
	DIType * pDitypeDerivedFrom = cast_or_null<DIType>(PMdnodeExtract(pLvalDerivedFrom));
	DIType * pDitypeVTableHolder = cast_or_null<DIType>(PMdnodeExtract(pLvalVTableHolder));

	auto pDibuild = unwrap(pDib);
	ArrayRef<Metadata *> ary(PPMetadataUnwrap(ppLvalElements, cElement), cElement);
	DINodeArray diaryElements = pDibuild->getOrCreateArray(ary);

	return wrap(pDibuild->createStructType(
								pDiscope,
								strrName,
								pDifile,
								nLine,
								cBitSize,
								cBitAlign,
								(DINode::DIFlags)nFlags,
								pDitypeDerivedFrom,
								diaryElements,
								nRuntimeLanguage,
								pDitypeVTableHolder));
}

LLVMValueRef LLVMDIBuilderCreateReplacableComposite(
    LLVMDIBuilderRef pDib, 
	unsigned nTag,
	LLVMValueRef pLvalScope, 
	const char * pChzName, 
	LLVMValueRef pLvalFile,
    unsigned nLine, 
	unsigned nRuntimeLanguage,
	uint64_t cBitSize,
	uint64_t cBitAlign,
	unsigned nFlags,
	const char * pChzUniqueName)
{
	DIScope * pDiscope = cast<DIScope>(PMdnodeExtract(pLvalScope));
	DIFile * pDifile = cast<DIFile>(PMdnodeExtract(pLvalFile));

	return wrap(unwrap(pDib)->createReplaceableCompositeType(
								nTag,
								StringRef(pChzName),
								pDiscope,
								pDifile, 
								nLine,
								nRuntimeLanguage,
								cBitSize,
								cBitAlign,
								(DINode::DIFlags)nFlags,
								StringRef(pChzUniqueName)));

}

void LLVMDIBuilderReplaceCompositeElements(
    LLVMDIBuilderRef pDib, 
	LLVMValueRef * ppLvalComposite,
    LLVMValueRef * ppLvalElements, 
	unsigned cElement)
{
	DICompositeType * pDicomp = cast<DICompositeType>(PMdnodeExtract(*ppLvalComposite));

	auto pDibuild = unwrap(pDib);
	ArrayRef<Metadata *> ary(PPMetadataUnwrap(ppLvalElements, cElement), cElement);
	DINodeArray diaryElements = pDibuild->getOrCreateArray(ary);

	pDibuild->replaceArrays(pDicomp, diaryElements);

	if (pDicomp->isTemporary())
		pDicomp = llvm::MDNode::replaceWithPermanent(llvm::TempDICompositeType(pDicomp));

	*ppLvalComposite = wrap(pDicomp);
}


LLVMValueRef LLVMDIBuilderGetOrCreateRange(LLVMDIBuilderRef pDib, int64_t iFirst, int64_t iLast)
{
	return wrap(unwrap(pDib)->getOrCreateSubrange(iFirst, iLast));
}

LLVMValueRef LLVMDIBuilderCreateArrayType(
	LLVMDIBuilderRef pDib,
	uint64_t cBitArraySize,
	uint64_t cBitAlign,
    LLVMValueRef pLvalElementType,
    LLVMValueRef * ppLvalSubscripts,
	unsigned cSubscript)
{
	auto pDibuild = unwrap(pDib);
	DIType * pDitypeElement = cast<DIType>(PMdnodeExtract(pLvalElementType));

	ArrayRef<Metadata *> ary(PPMetadataUnwrap(ppLvalSubscripts, cSubscript), cSubscript);
	DINodeArray diarySubscripts = pDibuild->getOrCreateArray(ary);

	return wrap(pDibuild->createArrayType( cBitArraySize, cBitAlign, pDitypeElement, diarySubscripts));
}

LLVMValueRef LLVMDIBuilderCreateEnumerationType(
    LLVMDIBuilderRef pDib, 
	LLVMValueRef pLvalScope, 
	const char * pChzName, 
	LLVMValueRef pLvalFile,
    unsigned nLine,
	uint64_t cBitSize,
	uint64_t cBitAlign,
    LLVMValueRef * ppLvalElements, 
	unsigned cElement,
	LLVMValueRef pLvalUnderlyingType)
{
	StringRef strrName(pChzName);
	DIScope * pDiscope = cast<DIScope>(PMdnodeExtract(pLvalScope));
	DIFile * pDifile = cast<DIFile>(PMdnodeExtract(pLvalFile));
	DIType * pDitypeUnderlying = cast<DIType>(PMdnodeExtract(pLvalUnderlyingType));

	auto pDibuild = unwrap(pDib);
	ArrayRef<Metadata *> ary(PPMetadataUnwrap(ppLvalElements, cElement), cElement);
	DINodeArray diaryElement = pDibuild->getOrCreateArray(ary);

	return wrap(pDibuild->createEnumerationType(pDiscope, strrName, pDifile, nLine, cBitSize, cBitAlign, diaryElement, pDitypeUnderlying));
}

LLVMValueRef LLVMDIBuilderCreateGlobalVariable(
	LLVMDIBuilderRef pDib,
	LLVMValueRef pLvalScope, 
	const char * pChzName,
	const char * pChzMangled,
	LLVMValueRef pLvalFile,
	unsigned nLine,
	LLVMValueRef pLvalType,
	bool fIsLocalToUnit,
	LLVMValueRef pLvalValue)
{
	StringRef strrName(pChzName);
	StringRef strrMangled(pChzMangled);
	DIScope * pDiscope = cast<DIScope>(PMdnodeExtract(pLvalScope));
	DIFile * pDifile = cast<DIFile>(PMdnodeExtract(pLvalFile));
	DIType * pDitype = cast<DIType>(PMdnodeExtract(pLvalType));

	llvm::GlobalVariable * pLglobvar = unwrap<GlobalVariable>(pLvalValue);

	llvm::DIGlobalVariableExpression * pLglobexp = unwrap(pDib)->createGlobalVariableExpression(
																	pDiscope,
																	strrName,
																	strrMangled,
																	pDifile,
																	nLine,
																	pDitype,
																	fIsLocalToUnit);

	pLglobvar->addDebugInfo(pLglobexp);
	return wrap(pLglobexp);

	/*
	return wrap(unwrap(pDib)->createGlobalVariableExpression(
								pDiscope,
								strrName,
								strrMangled,
								pDifile,
								nLine,
								pDitype,
								fIsLocalToUnit,
//								nullptr,
//								nullptr));
								PMdnodeExtract(pLvalValue)));
//								unwrap<MDNode>(pLvalValue)));
//								mdconst::extract<Constant>(pLvalValue)));
*/
}



LLVMValueRef LLVMDIBuilderCreateAutoVariable(
	LLVMDIBuilderRef pDib,
	LLVMValueRef pLvalScope, 
	const char * pChzName,
	LLVMValueRef pLvalFile,
	unsigned nLine,
	LLVMValueRef pLvalType,
	bool fIsPreservedWhenOptimized,
    unsigned nFlags)
{
	StringRef strrName(pChzName);
	DIScope * pDiscope = cast<DIScope>(PMdnodeExtract(pLvalScope));
	DIFile * pDifile = cast<DIFile>(PMdnodeExtract(pLvalFile));
	DIType * pDitype = cast<DIType>(PMdnodeExtract(pLvalType));

	return wrap(unwrap(pDib)->createAutoVariable(
								pDiscope,
								strrName,
								pDifile,
								nLine,
								pDitype,
								fIsPreservedWhenOptimized,
								(DINode::DIFlags)nFlags));
}

LLVMValueRef LLVMDIBuilderCreateParameterVariable(
	LLVMDIBuilderRef pDib,
	LLVMValueRef pLvalScope, 
	const char * pChzName,
	unsigned iArgument,	// argument index (1 relative, 0 if none)
	LLVMValueRef pLvalFile,
	unsigned nLine,
	LLVMValueRef pLvalType,
	bool fIsPreservedWhenOptimized,
    unsigned nFlags)
{
	StringRef strrName(pChzName);
	DIScope * pDiscope = cast<DIScope>(PMdnodeExtract(pLvalScope));
	DIFile * pDifile = cast<DIFile>(PMdnodeExtract(pLvalFile));
	DIType * pDitype = cast<DIType>(PMdnodeExtract(pLvalType));

	return wrap(unwrap(pDib)->createParameterVariable(
								pDiscope,
								strrName,
								iArgument,
								pDifile,
								nLine,
								pDitype,
								fIsPreservedWhenOptimized,
								(DINode::DIFlags)nFlags));
}

LLVMValueRef LLVMDIBuilderInsertDeclare(
	LLVMDIBuilderRef pDib,
	LLVMValueRef pLvalStorage,
	LLVMValueRef pLvalDIVariable,
	LLVMValueRef pLvalScope,
	unsigned iLine, 
	unsigned iColumn, 
	LLVMBasicBlockRef pLblock)
{
	auto pDibuild = unwrap(pDib);
	DIExpression * pDiExpression = pDibuild->createExpression(); // BB - fill this out when I need it

	Value * pLvalStore = unwrap(pLvalStorage);
	DILocalVariable * pDilocvar = cast<DILocalVariable>(PMdnodeExtract(pLvalDIVariable));
	BasicBlock * pLbb = unwrap(pLblock);

	DIScope * pDiscope = cast<DIScope>(PMdnodeExtract(pLvalScope));
	DILocation * pDiloc = llvm::DebugLoc::get(iLine, iColumn, pDiscope);

	return wrap(pDibuild->insertDeclare(pLvalStore, pDilocvar, pDiExpression, pDiloc, pLbb));
}

LLVMValueRef LLVMDIBuilderCreateFunctionType(
	LLVMDIBuilderRef pDib,
	LLVMValueRef * ppLvalParameters,
	unsigned cParameters,
    uint64_t cBitPointerSize,
    uint64_t cBitPointerAlign)
{
	auto pDibuild = unwrap(pDib);
	ArrayRef<Metadata *> ary(PPMetadataUnwrap(ppLvalParameters, cParameters), cParameters);
	DITypeRefArray diaryParameters = pDibuild->getOrCreateTypeArray(ary);


	LLVMValueRef pLvalDIType = wrap(unwrap(pDib)->createSubroutineType(diaryParameters));

	return LLVMDIBuilderCreatePointerType(pDib, pLvalDIType, cBitPointerSize, cBitPointerAlign, "");
}

LLVMValueRef LLVMDIBuilderCreateFunction(
    LLVMDIBuilderRef pDib,
	LLVMValueRef pLvalScope,
	const char * pChzName,
	const char * pChzMangled,
	LLVMValueRef pLvalFile,
	unsigned nLine,
	LLVMValueRef pLvalType,
	bool fIsLocalToUnit,
    bool fIsDefinition,
	unsigned nLineScopeBegin,
	unsigned nDwarfFlags,
	bool fIsOptimized,
    LLVMValueRef pLvalFunction,
	LLVMValueRef pLvalTemplateParm,
	LLVMValueRef pLvalDecl)
{
	StringRef strrName(pChzName);
	StringRef strrMangled(pChzMangled, (pChzMangled) ? strlen(pChzMangled) : 0);
	DIScope * pDiscope = cast<DIScope>(PMdnodeExtract(pLvalScope));
	DIFile * pDifile = cast<DIFile>(PMdnodeExtract(pLvalFile));

	DIDerivedType * pDideriveSub = cast<DIDerivedType>(PMdnodeExtract(pLvalType)); 
	DISubroutineType * pDisubt = cast<DISubroutineType>(pDideriveSub->getBaseType());
	Function * pFunc = unwrap<Function>(pLvalFunction);

	//MDNode * pMdnodeTemplateParm = PMdnodeExtract(pLvalTemplateParm);
	// BB - what's the right  thing here?
	DITemplateParameterArray tupelaryTemplateParm = cast_or_null<MDTuple>(PMdnodeExtract(pLvalTemplateParm));

	DISubprogram * pDisubDecl = cast_or_null<DISubprogram>(PMdnodeExtract(pLvalDecl));

	DISubprogram * pDisub = unwrap(pDib)->createFunction(
											pDiscope,
											strrName,
											strrMangled,
											pDifile,
											nLine,
											pDisubt,
											fIsLocalToUnit,
											fIsDefinition,
											nLineScopeBegin,
											(DIFile::DIFlags)nDwarfFlags, 
											fIsOptimized,
											tupelaryTemplateParm,
											pDisubDecl);

	pFunc->setSubprogram(pDisub);
	return wrap(pDisub);
}


LLVMValueRef LLVMDIBuilderCreateNamespace(
	LLVMDIBuilderRef pDib,
	LLVMValueRef pLvalScope,
	const char * pChzName,
	LLVMValueRef pLvalFile,
	unsigned nLine)
{
	StringRef strrName(pChzName);
	DIScope * pDiscope = cast<DIScope>(PMdnodeExtract(pLvalScope));

	return wrap(unwrap(pDib)->createNameSpace(pDiscope, strrName, true));
}

LLVMValueRef LLVMDIBuilderCreateLexicalBlock(
	LLVMDIBuilderRef pDib,
	LLVMValueRef pLvalScope,
	LLVMValueRef pLvalFile,
	unsigned nLine,
	unsigned nCol)
{
	DIScope * pDiscope = cast<DIScope>(PMdnodeExtract(pLvalScope));
	DIFile * pDifile = cast<DIFile>(PMdnodeExtract(pLvalFile));

	return wrap(unwrap(pDib)->createLexicalBlock(pDiscope, pDifile, nLine, nCol));
}
