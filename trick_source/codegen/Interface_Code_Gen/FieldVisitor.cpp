
#include <iostream>

#include "llvm/Support/CommandLine.h"
#include "clang/Basic/SourceManager.h"
#include "clang/AST/RecordLayout.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Comment.h"

#include "FieldVisitor.hh"
#include "FieldDescription.hh"
#include "ClassValues.hh"
#include "EnumValues.hh"
#include "ClassVisitor.hh"
#include "CommentSaver.hh"
#include "Utilities.hh"

extern llvm::cl::opt< int > debug_level ;

FieldVisitor::FieldVisitor(clang::CompilerInstance & in_ci ,
 HeaderSearchDirs & in_hsd ,
 CommentSaver & in_cs ,
 PrintAttributes & in_pa ,
 std::string container_class ,
 bool in_inherited ) :
  ci(in_ci) ,
  hsd(in_hsd) ,
  cs(in_cs) ,
  pa(in_pa) {
    fdes = new FieldDescription(container_class, in_inherited ) ;
}

bool FieldVisitor::VisitDecl(clang::Decl *d) {
    if ( debug_level >= 4 ) {
        std::cout << "\n\033[32mFieldVisitor VisitDecl Decl = " << d->getDeclKindName() << "\033[00m" << std::endl ;
        d->dump() ;
    }
    return true ;
}

bool FieldVisitor::VisitType(clang::Type *t) {
    if ( debug_level >= 4 ) {
        std::cout << "FieldVisitor VisitType Type = " << t->getTypeClassName() << std::endl ;
        t->dump() ;
    }
    // If this type is a reference, set IO to 0
    if ( t->isReferenceType() ) {
        if ( debug_level >= 3 ) {
            std::cout << "FieldVisitor VisitType found reference, setIO = 0 " << std::endl ;
        }
        fdes->setIO(0) ;
    }
    return true;
}

bool FieldVisitor::VisitBuiltinType(clang::BuiltinType *bt) {

    if ( debug_level >= 3 ) {
        std::cout << "FieldVisitor::VisitBuiltinType " << bt->desugar().getAsString() << std::endl ;
    }
    fdes->setTypeName(bt->desugar().getAsString()) ;
    if ( fdes->isBitField() ) {
        if ( bt->isUnsignedInteger() ) {
            fdes->setEnumString("TRICK_UNSIGNED_BITFIELD") ;
        } else {
            fdes->setEnumString("TRICK_BITFIELD") ;
        }
        if ( bt->getKind() == clang::BuiltinType::Bool ) {
            fdes->setTypeName("bool") ;
        }
    } else {
        switch ( bt->getKind() ) {
            case clang::BuiltinType::Bool:
                fdes->setEnumString("TRICK_BOOLEAN") ;
                fdes->setTypeName("bool") ;
                break ;
            case clang::BuiltinType::Char_S:
            case clang::BuiltinType::SChar:
                fdes->setEnumString("TRICK_CHARACTER") ;
                break ;
            case clang::BuiltinType::UChar:
            case clang::BuiltinType::Char_U:
                fdes->setEnumString("TRICK_UNSIGNED_CHARACTER") ;
                break ;
            case clang::BuiltinType::WChar_U:
            case clang::BuiltinType::WChar_S:
                fdes->setEnumString("TRICK_WCHAR") ;
                break ;
            case clang::BuiltinType::Short:
                fdes->setEnumString("TRICK_SHORT") ;
                break ;
            case clang::BuiltinType::UShort:
            case clang::BuiltinType::Char16:
                fdes->setEnumString("TRICK_UNSIGNED_SHORT") ;
                break ;
            case clang::BuiltinType::Int:
                fdes->setEnumString("TRICK_INTEGER") ;
                break ;
            case clang::BuiltinType::UInt:
                fdes->setEnumString("TRICK_UNSIGNED_INTEGER") ;
                break ;
            case clang::BuiltinType::Long:
                fdes->setEnumString("TRICK_LONG") ;
                break ;
            case clang::BuiltinType::ULong:
                fdes->setEnumString("TRICK_UNSIGNED_LONG") ;
                break ;
            case clang::BuiltinType::LongLong:
                fdes->setEnumString("TRICK_LONG_LONG") ;
                break ;
            case clang::BuiltinType::ULongLong:
                fdes->setEnumString("TRICK_UNSIGNED_LONG_LONG") ;
                break ;
            case clang::BuiltinType::Float:
                fdes->setEnumString("TRICK_FLOAT") ;
                break ;
            case clang::BuiltinType::Double:
                fdes->setEnumString("TRICK_DOUBLE") ;
                break ;
            default:
                fdes->setEnumString("TRICK_VOID") ;
                break ;
        }
    }
    return true;
}

bool FieldVisitor::VisitConstantArrayType(clang::ConstantArrayType *cat) {
    //cat->dump() ; std::cout << std::endl ;
    fdes->addArrayDim(cat->getSize().getZExtValue()) ;
    return true;
}

/* Both FieldDecl and VarDecl derive from DeclaratorDecl.  We can do
   common things to both node types in this function */
bool FieldVisitor::VisitDeclaratorDecl( clang::DeclaratorDecl *dd ) {

    fdes->setFileName(getFileName(ci , dd->getLocation(), hsd)) ;
    fdes->setName(dd->getNameAsString()) ;
    fdes->setAccess(dd->getAccess()) ;

    /* Get the source location of this field. */
    clang::SourceRange dd_range = dd->getSourceRange() ;
    std::string file_name = getFileName(ci, dd_range.getEnd(), hsd) ;
    if ( ! file_name.empty() ) {
        if ( isInUserOrTrickCode( ci , dd_range.getEnd() , hsd ) ) {
            fdes->setLineNo(ci.getSourceManager().getSpellingLineNumber(dd_range.getEnd())) ;
            /* process comment if neither ICG:(No) or ICG:(NoComment) is present */
            if (  cs.hasTrickHeader(file_name) and
                 !cs.hasICGNoComment(file_name) and
                 !hsd.isPathInICGNoComment(file_name) ) {
                /* Get the possible comment on this line and parse it */
                fdes->parseComment(cs.getComment(file_name , fdes->getLineNo())) ;
            }
        }
    }

    if ( debug_level >= 3 ) {
        if ( ! ci.getSourceManager().isInSystemHeader(dd_range.getEnd()) ) {
            std::cout << "FieldVisitor VisitDeclaratorDecl" << std::endl ;
            std::cout << "    file_name = " << file_name << std::endl ;
            std::cout << "    line num = " << fdes->getLineNo() << std::endl ;
            std::cout << "    comment = " << cs.getComment(file_name , fdes->getLineNo()) << std::endl ;
            std::cout << "    public/private = " << fdes->getAccess() << std::endl ;
            std::cout << "    io = " << fdes->getIO() << std::endl ;
        }
    }

    // returns true if any io is allowed. returning false will stop processing of this variable here.
    return fdes->getIO() ;
}

bool FieldVisitor::VisitEnumType( clang::EnumType *et ) {

    std::string enum_type_name = et->desugar().getAsString() ;
    if ( debug_level >= 3 ) {
        std::cout << "\nFieldVisitor VisitEnumType" << std::endl ;
        std::cout << et->desugar().getAsString() << std::endl ;
    }
    size_t pos ;
    if ((pos = enum_type_name.find("enum ")) != std::string::npos ) {
        enum_type_name.erase(pos , 5) ;
    }
    // If this enum is to an enumeration found inside a template, e.g. template<type>::enum_type ignore it.
    // because there will not be enumeration attribute information generated for this enum.
    if ((pos = enum_type_name.find("<")) != std::string::npos ) {
        size_t last_pos = enum_type_name.find_last_of(">::") ;
        enum_type_name.replace(pos, last_pos - pos + 1, "__") ;
        //fdes->setIO(0) ;
    }
    fdes->setMangledTypeName("") ;
    fdes->setTypeName(enum_type_name) ;
    fdes->setEnumString("TRICK_ENUMERATED") ;
    fdes->setEnum(true) ;
    return true ;
}

bool FieldVisitor::VisitFieldDecl( clang::FieldDecl *field ) {

    // set the offset to the field
    fdes->setFieldOffset(field->getASTContext().getFieldOffset(field) / 8) ;

    fdes->setBitField(field->isBitField()) ;
    if ( fdes->isBitField() ) {
        fdes->setBitFieldWidth(field->getBitWidthValue(field->getASTContext())) ;
        fdes->calcBitfieldOffset() ;
    }

    // If the current type is not canonical because of typedefs or template parameter substitution,
    // traverse the canonical type
    clang::QualType qt = field->getType() ;
    if ( debug_level >= 3 ) {
        std::cout << "FieldVisitor VisitFieldDecl" << std::endl ;
        std::cout << "    is_bitfield = " << fdes->isBitField() << std::endl ;
        std::cout << "    is_canonical = " << qt.isCanonical() << std::endl ;
        //field->dump() ;
    }

    // set the offset to the field
    fdes->setFieldWidth(field->getASTContext().getTypeSize(qt) / 8) ;

    if ( !qt.isCanonical() ) {
        fdes->setNonCanonicalTypeName(qt.getAsString()) ;
        clang::QualType ct = qt.getCanonicalType() ;
        std::string tst_string = ct.getAsString() ;
        if ( debug_level >= 3 ) {
            std::cout << "\033[33mFieldVisitor VisitFieldDecl: Processing canonical type\033[00m" << std::endl ;
            ct.dump() ;
        }
        TraverseType(ct) ;
        // We have extracted the canonical type and everything else we need
        // return false so we cut off processing of this AST branch
        return false ;
    }

    return true ;
}

bool FieldVisitor::VisitPointerType(clang::PointerType *p) {
    fdes->addArrayDim(-1) ;
    return true;
}

static std::string mangle_string( std::string in_name ) {
    // convert characters not valid in a function name to underscores
    std::string mangled_name = in_name ;
    // Create a mangled type name, some characters have to converted to underscores.
    std::replace( mangled_name.begin(), mangled_name.end(), '<', '_') ;
    std::replace( mangled_name.begin(), mangled_name.end(), '>', '_') ;
    std::replace( mangled_name.begin(), mangled_name.end(), ' ', '_') ;
    std::replace( mangled_name.begin(), mangled_name.end(), ',', '_') ;
    std::replace( mangled_name.begin(), mangled_name.end(), ':', '_') ;
    std::replace( mangled_name.begin(), mangled_name.end(), '*', '_') ;
    return mangled_name ;
}

std::map < std::string , std::string > FieldVisitor::processed_templates ;

bool FieldVisitor::ProcessTemplate(std::string in_name , clang::CXXRecordDecl * crd ) {

    // Save container namespaces and classes.
    fdes->getNamespacesAndClasses(crd->getDeclContext()) ;

    size_t pos ;

    // Check to see if we've processed this template before
    // If not we need to create attributes for this template
    if ( processed_templates.find(in_name) == processed_templates.end() ) {
        std::string mangled_name = mangle_string(in_name) ;

        // save off the mangled name of this template to be used if another variable is the same template type
        processed_templates[in_name] = fdes->getContainerClass() + "_" +
         fdes->getName() + "_" + mangled_name ;

        // Traverse the template declaration
        CXXRecordVisitor template_spec_cvis(ci , cs, hsd , pa, false, false, true) ;
        template_spec_cvis.get_class_data()->setMangledTypeName(processed_templates[in_name]) ;
        template_spec_cvis.TraverseCXXRecordDecl(crd) ;

        // Set the actual type name and file name. Print the attributes for this template type
        template_spec_cvis.get_class_data()->setName(in_name) ;
        template_spec_cvis.get_class_data()->setFileName(fdes->getFileName()) ;
        pa.printClass(template_spec_cvis.get_class_data()) ;

        if ( debug_level >= 4 ) {
            std::cout << "Added template class from FieldVisitor ProcessTemplate " ;
            std::cout << in_name << std::endl ;
            std::cout << *fdes << std::endl ;
        }
    }

    fdes->setMangledTypeName(processed_templates[in_name]) ;
    fdes->setEnumString("TRICK_STRUCTURED") ;
    fdes->setRecord(true) ;

    // processing the template will process the type, return false to stop processing
    return false ;
}

static std::map<std::string, bool> init_stl_classes() {
    std::map<std::string, bool> my_map ;
    my_map.insert(std::pair<std::string, bool>("std::deque", 1)) ;
    my_map.insert(std::pair<std::string, bool>("std::list", 1)) ;
    my_map.insert(std::pair<std::string, bool>("std::map", 1)) ;
    my_map.insert(std::pair<std::string, bool>("std::multiset", 1)) ;
    my_map.insert(std::pair<std::string, bool>("std::multimap", 1)) ;
    my_map.insert(std::pair<std::string, bool>("std::pair", 0)) ;
    my_map.insert(std::pair<std::string, bool>("std::priority_queue", 0)) ;
    my_map.insert(std::pair<std::string, bool>("std::queue", 0)) ;
    my_map.insert(std::pair<std::string, bool>("std::set", 1)) ;
    my_map.insert(std::pair<std::string, bool>("std::stack", 0)) ;
    my_map.insert(std::pair<std::string, bool>("std::vector", 1)) ;
    my_map.insert(std::pair<std::string, bool>("std::__1::deque", 1)) ;
    my_map.insert(std::pair<std::string, bool>("std::__1::list", 1)) ;
    my_map.insert(std::pair<std::string, bool>("std::__1::map", 1)) ;
    my_map.insert(std::pair<std::string, bool>("std::__1::multiset", 1)) ;
    my_map.insert(std::pair<std::string, bool>("std::__1::multimap", 1)) ;
    my_map.insert(std::pair<std::string, bool>("std::__1::pair", 0)) ;
    my_map.insert(std::pair<std::string, bool>("std::__1::priority_queue", 0)) ;
    my_map.insert(std::pair<std::string, bool>("std::__1::queue", 0)) ;
    my_map.insert(std::pair<std::string, bool>("std::__1::set", 1)) ;
    my_map.insert(std::pair<std::string, bool>("std::__1::stack", 0)) ;
    my_map.insert(std::pair<std::string, bool>("std::__1::vector", 1)) ;
    my_map.insert(std::pair<std::string, bool>("std::__cxx11::deque", 1)) ;
    my_map.insert(std::pair<std::string, bool>("std::__cxx11::list", 1)) ;
    my_map.insert(std::pair<std::string, bool>("std::__cxx11::map", 1)) ;
    my_map.insert(std::pair<std::string, bool>("std::__cxx11::multiset", 1)) ;
    my_map.insert(std::pair<std::string, bool>("std::__cxx11::multimap", 1)) ;
    my_map.insert(std::pair<std::string, bool>("std::__cxx11::pair", 0)) ;
    my_map.insert(std::pair<std::string, bool>("std::__cxx11::priority_queue", 0)) ;
    my_map.insert(std::pair<std::string, bool>("std::__cxx11::queue", 0)) ;
    my_map.insert(std::pair<std::string, bool>("std::__cxx11::set", 1)) ;
    my_map.insert(std::pair<std::string, bool>("std::__cxx11::stack", 0)) ;
    my_map.insert(std::pair<std::string, bool>("std::__cxx11::vector", 1)) ;
    return my_map ;
}

static std::map<std::string, bool> stl_classes = init_stl_classes() ;

bool FieldVisitor::VisitRecordType(clang::RecordType *rt) {
    if ( debug_level >= 3 ) {
        std::cout << "FieldVisitor VisitRecordType" << std::endl ;
        rt->dump() ;
    }
    /* String types are typed as records but we treat them differently.
       The attributes type is set to TRICK_STRING instead of TRICK_STRUCTURE.
       The type is set to std::string.  We can return false here to stop processing of this type. */
    std::string type_name = rt->getDecl()->getQualifiedNameAsString() ;
    if ( ! type_name.compare("std::basic_string") || !type_name.compare("std::__1::basic_string") || 
         ! type_name.compare("std::__cxx11::basic_string") ) {
        fdes->setEnumString("TRICK_STRING") ;
        fdes->setTypeName("std::string") ;
        return false ;
    }

    std::string tst_string = rt->desugar().getAsString() ;
    // remove class keyword if it exists
    size_t pos ;
    while ((pos = tst_string.find("class ")) != std::string::npos ) {
        tst_string.erase(pos , 6) ;
    }
    while ((pos = tst_string.find("struct ")) != std::string::npos ) {
        tst_string.erase(pos , 7) ;
    }
    // clang changes bool to _Bool.  We need to change it back
    if ((pos = tst_string.find("<_Bool")) != std::string::npos ) {
        tst_string.replace(pos , 6, "<bool") ;
    }
    while ((pos = tst_string.find(" _Bool")) != std::string::npos ) {
        tst_string.replace(pos , 6, " bool") ;
    }
    // NOTE: clang also changes FILE * to struct _SFILE *.  We may need to change that too.

    // Test if we have some type from STL.
    if (!tst_string.compare( 0 , 5 , "std::")) {
        // If we have some type from std, figure out if it is one we support.
        for ( std::map<std::string, bool>::iterator it = stl_classes.begin() ; it != stl_classes.end() ; it++ ) {
            /* Mark STL types that are not strings and exit */
            if (!tst_string.compare( 0 , (*it).first.size() , (*it).first)) {
                fdes->setEnumString("TRICK_STL") ;
                fdes->setSTL(true) ;
                fdes->setTypeName(tst_string) ;
                fdes->setSTLClear((*it).second) ;
                // set the type name to the non canonical name, the name the user put in the header file
                // The typename is not used by STL variables, and it is nice to see the type that was
                // actually inputted by the user
                fdes->setMangledTypeName(fdes->getNonCanonicalTypeName()) ;
                return false ;
            }
        }
        // If the record type is in std:: but not one we can process, set the I/O spec to zero and return.
        fdes->setIO(0) ;
        return false ;
    }

    /* Template specialization types will be processed here because the canonical type
       will be typed as a record.  We test if we have a template specialization type.
       If so process the template type and return */
    clang::RecordDecl * rd = rt->getDecl()->getDefinition() ;
    if ( rd != NULL and clang::ClassTemplateSpecializationDecl::classof(rd) ) {
        if ( debug_level >= 3 ) {
            rd->dump() ;
            std::cout << "    tst_string = " << tst_string << std::endl ;
            std::cout << "    rd is_a_template_specialization = " <<
             clang::ClassTemplateSpecializationDecl::classof(rd) << std::endl ;
        }
        return ProcessTemplate(tst_string, clang::cast<clang::CXXRecordDecl>(rd)) ;
    }

    /* Test to see if we have an embedded anonymous struct/union.  e.g. SB is anonymous below.
       struct SA {
          struct {
             double d ;
          } SB ;
       } ;
    */
    //std::cout << "hasNameForLinkage " << rt->getDecl()->hasNameForLinkage() << std::endl ;
    if ( rt->getDecl()->hasNameForLinkage() ) {
        if ( rt->getDecl()->getDeclName() ) {
            //std::cout << "getDeclName " << type_name << std::endl ;
            fdes->setTypeName(type_name) ;
        } else {
            //std::cout << "getTypedefNameForAnonDecl " << rt->getDecl()->getTypedefNameForAnonDecl() << std::endl ;
            fdes->setTypeName(rt->getDecl()->getTypedefNameForAnonDecl()->getQualifiedNameAsString()) ;
        }
    } else {
        // io_src code not possible for anonymous struct/unions.  Set the I/O to 0 to ignore it.
        if ( debug_level >= 3 ) {
            std::cout << "FieldVisitor VisitRecordType found anonymous type, setIO = 0" << std::endl ;
        }
        fdes->setIO(0) ;
    }

    fdes->setEnumString("TRICK_STRUCTURED") ;
    fdes->setRecord(true) ;
    // We have our type, return false to stop processing this AST branch
    return false;
}

bool FieldVisitor::VisitVarDecl( clang::VarDecl *v ) {
    fdes->setStatic(v->isStaticDataMember()) ;
    /* If we have a static const integer type with an initializer value, this variable will
       not be instantiated by the compiler. The compiler substitutes in the value internally.
       set the IO to 0 to stop attribute printing */
    // Note: gcc allows an initializer for floating point types too.
    if ( v->isStaticDataMember() and
         v->getType().isConstQualified() and
         v->hasInit() ) {
        fdes->setIO(0) ;
    } else if ( v->isStaticDataMember() and
         v->getType().isConstQualified() ) {
        /* Static const members cannot be set through attributes code. Remove input
           capabilities by taking current io specification & 1 */
        fdes->setIO(fdes->getIO() & 1) ;
    }
    if ( debug_level >= 3 ) {
        std::cout << "FieldVisitor VisitVarDecl " << fdes->getName() << std::endl ;
        std::cout << "    is static = " << fdes->isStatic() << std::endl ;
        std::cout << "    is const = " << v->getType().isConstQualified() << std::endl ;
        std::cout << "    has initializer value = " << v->hasInit() << std::endl ;
        std::cout << "    IO = " << fdes->getIO() << std::endl ;
        //v->dump() ; std::cout << std::endl ;
    }
    return true ;
}

FieldDescription * FieldVisitor::get_field_data() {
    return fdes ;
}

