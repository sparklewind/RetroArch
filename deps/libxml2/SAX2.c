/*
 * SAX2.c : Default SAX2 handler to build a tree.
 *
 * See Copyright for the status of this software.
 *
 * Daniel Veillard <daniel@veillard.com>
 */


#define IN_LIBXML
#include "libxml.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <libxml/xmlmemory.h>
#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/parserInternals.h>
#include <libxml/valid.h>
#include <libxml/entities.h>
#include <libxml/xmlerror.h>
#include <libxml/xmlIO.h>
#include <libxml/SAX.h>
#include <libxml/uri.h>
#include <libxml/valid.h>
#include <libxml/globals.h>

/* Define SIZE_T_MAX unless defined through <limits.h>. */
#ifndef SIZE_T_MAX
# define SIZE_T_MAX     ((size_t)-1)
#endif /* !SIZE_T_MAX */

/**
 * TODO:
 *
 * macro to flag unimplemented blocks
 * XML_CATALOG_PREFER user env to select between system/public prefered
 * option. C.f. Richard Tobin <richard@cogsci.ed.ac.uk>
 *> Just FYI, I am using an environment variable XML_CATALOG_PREFER with
 *> values "system" and "public".  I have made the default be "system" to
 *> match yours.
 */
#define TODO 								\
    xmlGenericError(xmlGenericErrorContext,				\
	    "Unimplemented block at %s:%d\n",				\
            __FILE__, __LINE__);

/*
 * xmlSAX2ErrMemory:
 * @ctxt:  an XML validation parser context
 * @msg:   a string to accompany the error message
 */
static void
xmlSAX2ErrMemory(xmlParserCtxtPtr ctxt, const char *msg) {
    if (ctxt != NULL) {
	if ((ctxt->sax != NULL) && (ctxt->sax->error != NULL))
	    ctxt->sax->error(ctxt->userData, "%s: out of memory\n", msg);
	ctxt->errNo = XML_ERR_NO_MEMORY;
	ctxt->instate = XML_PARSER_EOF;
	ctxt->disableSAX = 1;
    }
}

/**
 * xmlValidError:
 * @ctxt:  an XML validation parser context
 * @error:  the error number
 * @msg:  the error message
 * @str1:  extra data
 * @str2:  extra data
 *
 * Handle a validation error
 */
static void
SAXxmlErrValid(xmlParserCtxtPtr ctxt, xmlParserErrors error,
            const char *msg, const char *str1, const char *str2)
{
    xmlStructuredErrorFunc schannel = NULL;

    if ((ctxt != NULL) && (ctxt->disableSAX != 0) &&
        (ctxt->instate == XML_PARSER_EOF))
	return;
    if (ctxt != NULL) {
	ctxt->errNo = error;
	if ((ctxt->sax != NULL) && (ctxt->sax->initialized == XML_SAX2_MAGIC))
	    schannel = ctxt->sax->serror;
	__xmlRaiseError(schannel,
			ctxt->vctxt.error, ctxt->vctxt.userData,
			ctxt, NULL, XML_FROM_DTD, error,
			XML_ERR_ERROR, NULL, 0, (const char *) str1,
			(const char *) str2, NULL, 0, 0,
			msg, (const char *) str1, (const char *) str2);
	ctxt->valid = 0;
    } else {
	__xmlRaiseError(schannel,
			NULL, NULL,
			ctxt, NULL, XML_FROM_DTD, error,
			XML_ERR_ERROR, NULL, 0, (const char *) str1,
			(const char *) str2, NULL, 0, 0,
			msg, (const char *) str1, (const char *) str2);
    }
}

/**
 * SAXxmlFatalErrMsg:
 * @ctxt:  an XML parser context
 * @error:  the error number
 * @msg:  the error message
 * @str1:  an error string
 * @str2:  an error string
 *
 * Handle a fatal parser error, i.e. violating Well-Formedness constraints
 */
static void
SAXxmlFatalErrMsg(xmlParserCtxtPtr ctxt, xmlParserErrors error,
               const char *msg, const xmlChar *str1, const xmlChar *str2)
{
    if ((ctxt != NULL) && (ctxt->disableSAX != 0) &&
        (ctxt->instate == XML_PARSER_EOF))
	return;
    if (ctxt != NULL)
	ctxt->errNo = error;
    __xmlRaiseError(NULL, NULL, NULL, ctxt, NULL, XML_FROM_PARSER, error,
                    XML_ERR_FATAL, NULL, 0, 
		    (const char *) str1, (const char *) str2,
		    NULL, 0, 0, msg, str1, str2);
    if (ctxt != NULL) {
	ctxt->wellFormed = 0;
	ctxt->valid = 0;
	if (ctxt->recovery == 0)
	    ctxt->disableSAX = 1;
    }
}

/**
 * xmlWarnMsg:
 * @ctxt:  an XML parser context
 * @error:  the error number
 * @msg:  the error message
 * @str1:  an error string
 * @str2:  an error string
 *
 * Handle a parser warning
 */
static void
xmlWarnMsg(xmlParserCtxtPtr ctxt, xmlParserErrors error,
               const char *msg, const xmlChar *str1)
{
    if ((ctxt != NULL) && (ctxt->disableSAX != 0) &&
        (ctxt->instate == XML_PARSER_EOF))
	return;
    if (ctxt != NULL)
	ctxt->errNo = error;
    __xmlRaiseError(NULL, NULL, NULL, ctxt, NULL, XML_FROM_PARSER, error,
                    XML_ERR_WARNING, NULL, 0, 
		    (const char *) str1, NULL,
		    NULL, 0, 0, msg, str1);
}

/**
 * xmlNsErrMsg:
 * @ctxt:  an XML parser context
 * @error:  the error number
 * @msg:  the error message
 * @str1:  an error string
 * @str2:  an error string
 *
 * Handle a namespace error
 */
static void
xmlNsErrMsg(xmlParserCtxtPtr ctxt, xmlParserErrors error,
            const char *msg, const xmlChar *str1, const xmlChar *str2)
{
    if ((ctxt != NULL) && (ctxt->disableSAX != 0) &&
        (ctxt->instate == XML_PARSER_EOF))
	return;
    if (ctxt != NULL)
	ctxt->errNo = error;
    __xmlRaiseError(NULL, NULL, NULL, ctxt, NULL, XML_FROM_NAMESPACE, error,
                    XML_ERR_ERROR, NULL, 0, 
		    (const char *) str1, (const char *) str2,
		    NULL, 0, 0, msg, str1, str2);
}

/**
 * xmlNsWarnMsg:
 * @ctxt:  an XML parser context
 * @error:  the error number
 * @msg:  the error message
 * @str1:  an error string
 *
 * Handle a namespace warning
 */
static void
xmlNsWarnMsg(xmlParserCtxtPtr ctxt, xmlParserErrors error,
             const char *msg, const xmlChar *str1, const xmlChar *str2)
{
    if ((ctxt != NULL) && (ctxt->disableSAX != 0) &&
        (ctxt->instate == XML_PARSER_EOF))
	return;
    if (ctxt != NULL)
	ctxt->errNo = error;
    __xmlRaiseError(NULL, NULL, NULL, ctxt, NULL, XML_FROM_NAMESPACE, error,
                    XML_ERR_WARNING, NULL, 0, 
		    (const char *) str1, (const char *) str2,
		    NULL, 0, 0, msg, str1, str2);
}

/**
 * xmlSAX2GetPublicId:
 * @ctx: the user data (XML parser context)
 *
 * Provides the public ID e.g. "-//SGMLSOURCE//DTD DEMO//EN"
 *
 * Returns a xmlChar *
 */
const xmlChar *
xmlSAX2GetPublicId(void *ctx ATTRIBUTE_UNUSED)
{
    /* xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr) ctx; */
    return(NULL);
}

/**
 * xmlSAX2GetSystemId:
 * @ctx: the user data (XML parser context)
 *
 * Provides the system ID, basically URL or filename e.g.
 * http://www.sgmlsource.com/dtds/memo.dtd
 *
 * Returns a xmlChar *
 */
const xmlChar *
xmlSAX2GetSystemId(void *ctx)
{
    xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr) ctx;
    if ((ctx == NULL) || (ctxt->input == NULL)) return(NULL);
    return((const xmlChar *) ctxt->input->filename); 
}

/**
 * xmlSAX2GetLineNumber:
 * @ctx: the user data (XML parser context)
 *
 * Provide the line number of the current parsing point.
 *
 * Returns an int
 */
int
xmlSAX2GetLineNumber(void *ctx)
{
    xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr) ctx;
    if ((ctx == NULL) || (ctxt->input == NULL)) return(0);
    return(ctxt->input->line);
}

/**
 * xmlSAX2GetColumnNumber:
 * @ctx: the user data (XML parser context)
 *
 * Provide the column number of the current parsing point.
 *
 * Returns an int
 */
int
xmlSAX2GetColumnNumber(void *ctx)
{
    xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr) ctx;
    if ((ctx == NULL) || (ctxt->input == NULL)) return(0);
    return(ctxt->input->col);
}

/**
 * xmlSAX2IsStandalone:
 * @ctx: the user data (XML parser context)
 *
 * Is this document tagged standalone ?
 *
 * Returns 1 if true
 */
int
xmlSAX2IsStandalone(void *ctx)
{
    xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr) ctx;
    if ((ctx == NULL) || (ctxt->myDoc == NULL)) return(0);
    return(ctxt->myDoc->standalone == 1);
}

/**
 * xmlSAX2HasInternalSubset:
 * @ctx: the user data (XML parser context)
 *
 * Does this document has an internal subset
 *
 * Returns 1 if true
 */
int
xmlSAX2HasInternalSubset(void *ctx)
{
    xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr) ctx;
    if ((ctxt == NULL) || (ctxt->myDoc == NULL)) return(0);
    return(ctxt->myDoc->intSubset != NULL);
}

/**
 * xmlSAX2HasExternalSubset:
 * @ctx: the user data (XML parser context)
 *
 * Does this document has an external subset
 *
 * Returns 1 if true
 */
int
xmlSAX2HasExternalSubset(void *ctx)
{
    xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr) ctx;
    if ((ctxt == NULL) || (ctxt->myDoc == NULL)) return(0);
    return(ctxt->myDoc->extSubset != NULL);
}

/**
 * xmlSAX2InternalSubset:
 * @ctx:  the user data (XML parser context)
 * @name:  the root element name
 * @ExternalID:  the external ID
 * @SystemID:  the SYSTEM ID (e.g. filename or URL)
 *
 * Callback on internal subset declaration.
 */
void
xmlSAX2InternalSubset(void *ctx, const xmlChar *name,
	       const xmlChar *ExternalID, const xmlChar *SystemID)
{
    xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr) ctx;
    xmlDtdPtr dtd;
    if (ctx == NULL) return;

    if (ctxt->myDoc == NULL)
	return;
    dtd = xmlGetIntSubset(ctxt->myDoc);
    if (dtd != NULL) {
	if (ctxt->html)
	    return;
	xmlUnlinkNode((xmlNodePtr) dtd);
	xmlFreeDtd(dtd);
	ctxt->myDoc->intSubset = NULL;
    }
    ctxt->myDoc->intSubset = 
	xmlCreateIntSubset(ctxt->myDoc, name, ExternalID, SystemID);
    if (ctxt->myDoc->intSubset == NULL)
        xmlSAX2ErrMemory(ctxt, "xmlSAX2InternalSubset");
}

/**
 * xmlSAX2ExternalSubset:
 * @ctx: the user data (XML parser context)
 * @name:  the root element name
 * @ExternalID:  the external ID
 * @SystemID:  the SYSTEM ID (e.g. filename or URL)
 *
 * Callback on external subset declaration.
 */
void
xmlSAX2ExternalSubset(void *ctx, const xmlChar *name,
	       const xmlChar *ExternalID, const xmlChar *SystemID)
{
    xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr) ctx;
    if (ctx == NULL) return;
    if (((ExternalID != NULL) || (SystemID != NULL)) &&
        (((ctxt->validate) || (ctxt->loadsubset != 0)) &&
	 (ctxt->wellFormed && ctxt->myDoc))) {
	/*
	 * Try to fetch and parse the external subset.
	 */
	xmlParserInputPtr oldinput;
	int oldinputNr;
	int oldinputMax;
	xmlParserInputPtr *oldinputTab;
	xmlParserInputPtr input = NULL;
	xmlCharEncoding enc;
	int oldcharset;

	/*
	 * Ask the Entity resolver to load the damn thing
	 */
	if ((ctxt->sax != NULL) && (ctxt->sax->resolveEntity != NULL))
	    input = ctxt->sax->resolveEntity(ctxt->userData, ExternalID,
	                                        SystemID);
	if (input == NULL) {
	    return;
	}

	xmlNewDtd(ctxt->myDoc, name, ExternalID, SystemID);

	/*
	 * make sure we won't destroy the main document context
	 */
	oldinput = ctxt->input;
	oldinputNr = ctxt->inputNr;
	oldinputMax = ctxt->inputMax;
	oldinputTab = ctxt->inputTab;
	oldcharset = ctxt->charset;

	ctxt->inputTab = (xmlParserInputPtr *)
	                 xmlMalloc(5 * sizeof(xmlParserInputPtr));
	if (ctxt->inputTab == NULL) {
	    xmlSAX2ErrMemory(ctxt, "xmlSAX2ExternalSubset");
	    ctxt->input = oldinput;
	    ctxt->inputNr = oldinputNr;
	    ctxt->inputMax = oldinputMax;
	    ctxt->inputTab = oldinputTab;
	    ctxt->charset = oldcharset;
	    return;
	}
	ctxt->inputNr = 0;
	ctxt->inputMax = 5;
	ctxt->input = NULL;
	xmlPushInput(ctxt, input);

	/*
	 * On the fly encoding conversion if needed
	 */
	if (ctxt->input->length >= 4) {
	    enc = xmlDetectCharEncoding(ctxt->input->cur, 4);
	    xmlSwitchEncoding(ctxt, enc);
	}

	if (input->filename == NULL)
	    input->filename = (char *) xmlCanonicPath(SystemID);
	input->line = 1;
	input->col = 1;
	input->base = ctxt->input->cur;
	input->cur = ctxt->input->cur;
	input->free = NULL;

	/*
	 * let's parse that entity knowing it's an external subset.
	 */
	xmlParseExternalSubset(ctxt, ExternalID, SystemID);

        /*
	 * Free up the external entities
	 */

	while (ctxt->inputNr > 1)
	    xmlPopInput(ctxt);
	xmlFreeInputStream(ctxt->input);
        xmlFree(ctxt->inputTab);

	/*
	 * Restore the parsing context of the main entity
	 */
	ctxt->input = oldinput;
	ctxt->inputNr = oldinputNr;
	ctxt->inputMax = oldinputMax;
	ctxt->inputTab = oldinputTab;
	ctxt->charset = oldcharset;
	/* ctxt->wellFormed = oldwellFormed; */
    }
}

/**
 * xmlSAX2ResolveEntity:
 * @ctx: the user data (XML parser context)
 * @publicId: The public ID of the entity
 * @systemId: The system ID of the entity
 *
 * The entity loader, to control the loading of external entities,
 * the application can either:
 *    - override this xmlSAX2ResolveEntity() callback in the SAX block
 *    - or better use the xmlSetExternalEntityLoader() function to
 *      set up it's own entity resolution routine
 *
 * Returns the xmlParserInputPtr if inlined or NULL for DOM behaviour.
 */
xmlParserInputPtr
xmlSAX2ResolveEntity(void *ctx, const xmlChar *publicId, const xmlChar *systemId)
{
    xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr) ctx;
    xmlParserInputPtr ret;
    xmlChar *URI;
    const char *base = NULL;

    if (ctx == NULL) return(NULL);
    if (ctxt->input != NULL)
	base = ctxt->input->filename;
    if (base == NULL)
	base = ctxt->directory;

    URI = xmlBuildURI(systemId, (const xmlChar *) base);

    ret = xmlLoadExternalEntity((const char *) URI,
				(const char *) publicId, ctxt);
    if (URI != NULL)
	xmlFree(URI);
    return(ret);
}

/**
 * xmlSAX2GetEntity:
 * @ctx: the user data (XML parser context)
 * @name: The entity name
 *
 * Get an entity by name
 *
 * Returns the xmlEntityPtr if found.
 */
xmlEntityPtr
xmlSAX2GetEntity(void *ctx, const xmlChar *name)
{
    xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr) ctx;
    xmlEntityPtr ret = NULL;

    if (ctx == NULL) return(NULL);

    if (ctxt->inSubset == 0) {
	ret = xmlGetPredefinedEntity(name);
	if (ret != NULL)
	    return(ret);
    }
    if ((ctxt->myDoc != NULL) && (ctxt->myDoc->standalone == 1)) {
	if (ctxt->inSubset == 2) {
	    ctxt->myDoc->standalone = 0;
	    ret = xmlGetDocEntity(ctxt->myDoc, name);
	    ctxt->myDoc->standalone = 1;
	} else {
	    ret = xmlGetDocEntity(ctxt->myDoc, name);
	    if (ret == NULL) {
		ctxt->myDoc->standalone = 0;
		ret = xmlGetDocEntity(ctxt->myDoc, name);
		if (ret != NULL) {
		    SAXxmlFatalErrMsg(ctxt, XML_ERR_NOT_STANDALONE,
	 "Entity(%s) document marked standalone but requires external subset\n",
				   name, NULL);
		}
		ctxt->myDoc->standalone = 1;
	    }
	}
    } else {
	ret = xmlGetDocEntity(ctxt->myDoc, name);
    }
    if ((ret != NULL) &&
	((ctxt->validate) || (ctxt->replaceEntities)) &&
	(ret->children == NULL) &&
	(ret->etype == XML_EXTERNAL_GENERAL_PARSED_ENTITY)) {
	int val;

	/*
	 * for validation purposes we really need to fetch and
	 * parse the external entity
	 */
	xmlNodePtr children;

        val = xmlParseCtxtExternalEntity(ctxt, ret->URI,
		                         ret->ExternalID, &children);
	if (val == 0) {
	    xmlAddChildList((xmlNodePtr) ret, children);
	} else {
	    SAXxmlFatalErrMsg(ctxt, XML_ERR_ENTITY_PROCESSING,
		           "Failure to process entity %s\n", name, NULL);
	    ctxt->validate = 0;
	    return(NULL);
	}
	ret->owner = 1;
	if (ret->checked == 0)
	    ret->checked = 1;
    }
    return(ret);
}

/**
 * xmlSAX2GetParameterEntity:
 * @ctx: the user data (XML parser context)
 * @name: The entity name
 *
 * Get a parameter entity by name
 *
 * Returns the xmlEntityPtr if found.
 */
xmlEntityPtr
xmlSAX2GetParameterEntity(void *ctx, const xmlChar *name)
{
    xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr) ctx;
    xmlEntityPtr ret;

    if (ctx == NULL) return(NULL);

    ret = xmlGetParameterEntity(ctxt->myDoc, name);
    return(ret);
}


/**
 * xmlSAX2EntityDecl:
 * @ctx: the user data (XML parser context)
 * @name:  the entity name 
 * @type:  the entity type 
 * @publicId: The public ID of the entity
 * @systemId: The system ID of the entity
 * @content: the entity value (without processing).
 *
 * An entity definition has been parsed
 */
void
xmlSAX2EntityDecl(void *ctx, const xmlChar *name, int type,
          const xmlChar *publicId, const xmlChar *systemId, xmlChar *content)
{
    xmlEntityPtr ent;
    xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr) ctx;

    if (ctx == NULL) return;
    if (ctxt->inSubset == 1) {
	ent = xmlAddDocEntity(ctxt->myDoc, name, type, publicId,
		              systemId, content);
	if ((ent == NULL) && (ctxt->pedantic))
	    xmlWarnMsg(ctxt, XML_WAR_ENTITY_REDEFINED,
	     "Entity(%s) already defined in the internal subset\n",
	               name);
	if ((ent != NULL) && (ent->URI == NULL) && (systemId != NULL)) {
	    xmlChar *URI;
	    const char *base = NULL;

	    if (ctxt->input != NULL)
		base = ctxt->input->filename;
	    if (base == NULL)
		base = ctxt->directory;
	
	    URI = xmlBuildURI(systemId, (const xmlChar *) base);
	    ent->URI = URI;
	}
    } else if (ctxt->inSubset == 2) {
	ent = xmlAddDtdEntity(ctxt->myDoc, name, type, publicId,
		              systemId, content);
	if ((ent == NULL) && (ctxt->pedantic) &&
	    (ctxt->sax != NULL) && (ctxt->sax->warning != NULL))
	    ctxt->sax->warning(ctxt->userData, 
	     "Entity(%s) already defined in the external subset\n", name);
	if ((ent != NULL) && (ent->URI == NULL) && (systemId != NULL)) {
	    xmlChar *URI;
	    const char *base = NULL;

	    if (ctxt->input != NULL)
		base = ctxt->input->filename;
	    if (base == NULL)
		base = ctxt->directory;
	
	    URI = xmlBuildURI(systemId, (const xmlChar *) base);
	    ent->URI = URI;
	}
    } else {
	SAXxmlFatalErrMsg(ctxt, XML_ERR_ENTITY_PROCESSING,
	               "SAX.xmlSAX2EntityDecl(%s) called while not in subset\n",
		       name, NULL);
    }
}

/**
 * xmlSAX2AttributeDecl:
 * @ctx: the user data (XML parser context)
 * @elem:  the name of the element
 * @fullname:  the attribute name 
 * @type:  the attribute type 
 * @def:  the type of default value
 * @defaultValue: the attribute default value
 * @tree:  the tree of enumerated value set
 *
 * An attribute definition has been parsed
 */
void
xmlSAX2AttributeDecl(void *ctx, const xmlChar *elem, const xmlChar *fullname,
              int type, int def, const xmlChar *defaultValue,
	      xmlEnumerationPtr tree)
{
    xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr) ctx;
    xmlAttributePtr attr;
    xmlChar *name = NULL, *prefix = NULL;

    if ((ctxt == NULL) || (ctxt->myDoc == NULL))
        return;

    if ((xmlStrEqual(fullname, BAD_CAST "xml:id")) &&
        (type != XML_ATTRIBUTE_ID)) {
	/*
	 * Raise the error but keep the validity flag
	 */
	int tmp = ctxt->valid;
	SAXxmlErrValid(ctxt, XML_DTD_XMLID_TYPE,
	      "xml:id : attribute type should be ID\n", NULL, NULL);
	ctxt->valid = tmp;
    }
    /* TODO: optimize name/prefix allocation */
    name = xmlSplitQName(ctxt, fullname, &prefix);
    ctxt->vctxt.valid = 1;
    if (ctxt->inSubset == 1)
	attr = xmlAddAttributeDecl(&ctxt->vctxt, ctxt->myDoc->intSubset, elem,
	       name, prefix, (xmlAttributeType) type,
	       (xmlAttributeDefault) def, defaultValue, tree);
    else if (ctxt->inSubset == 2)
	attr = xmlAddAttributeDecl(&ctxt->vctxt, ctxt->myDoc->extSubset, elem,
	   name, prefix, (xmlAttributeType) type, 
	   (xmlAttributeDefault) def, defaultValue, tree);
    else {
        SAXxmlFatalErrMsg(ctxt, XML_ERR_INTERNAL_ERROR,
	     "SAX.xmlSAX2AttributeDecl(%s) called while not in subset\n",
	               name, NULL);
	xmlFreeEnumeration(tree);
	return;
    }
    if (prefix != NULL)
	xmlFree(prefix);
    if (name != NULL)
	xmlFree(name);
}

/**
 * xmlSAX2ElementDecl:
 * @ctx: the user data (XML parser context)
 * @name:  the element name 
 * @type:  the element type 
 * @content: the element value tree
 *
 * An element definition has been parsed
 */
void
xmlSAX2ElementDecl(void *ctx, const xmlChar * name, int type,
            xmlElementContentPtr content)
{
    xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr) ctx;
    xmlElementPtr elem = NULL;

    if ((ctxt == NULL) || (ctxt->myDoc == NULL))
        return;


    if (ctxt->inSubset == 1)
        elem = xmlAddElementDecl(&ctxt->vctxt, ctxt->myDoc->intSubset,
                                 name, (xmlElementTypeVal) type, content);
    else if (ctxt->inSubset == 2)
        elem = xmlAddElementDecl(&ctxt->vctxt, ctxt->myDoc->extSubset,
                                 name, (xmlElementTypeVal) type, content);
    else {
        SAXxmlFatalErrMsg(ctxt, XML_ERR_INTERNAL_ERROR,
	     "SAX.xmlSAX2ElementDecl(%s) called while not in subset\n",
	               name, NULL);
        return;
    }
}

/**
 * xmlSAX2NotationDecl:
 * @ctx: the user data (XML parser context)
 * @name: The name of the notation
 * @publicId: The public ID of the entity
 * @systemId: The system ID of the entity
 *
 * What to do when a notation declaration has been parsed.
 */
void
xmlSAX2NotationDecl(void *ctx, const xmlChar *name,
	     const xmlChar *publicId, const xmlChar *systemId)
{
    xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr) ctx;
    xmlNotationPtr nota = NULL;

    if ((ctxt == NULL) || (ctxt->myDoc == NULL))
        return;


    if ((publicId == NULL) && (systemId == NULL)) {
	SAXxmlFatalErrMsg(ctxt, XML_ERR_NOTATION_PROCESSING,
	     "SAX.xmlSAX2NotationDecl(%s) externalID or PublicID missing\n",
	               name, NULL);
	return;
    } else if (ctxt->inSubset == 1)
	nota = xmlAddNotationDecl(&ctxt->vctxt, ctxt->myDoc->intSubset, name,
                              publicId, systemId);
    else if (ctxt->inSubset == 2)
	nota = xmlAddNotationDecl(&ctxt->vctxt, ctxt->myDoc->extSubset, name,
                              publicId, systemId);
    else {
	SAXxmlFatalErrMsg(ctxt, XML_ERR_NOTATION_PROCESSING,
	     "SAX.xmlSAX2NotationDecl(%s) called while not in subset\n",
	               name, NULL);
	return;
    }
}

/**
 * xmlSAX2UnparsedEntityDecl:
 * @ctx: the user data (XML parser context)
 * @name: The name of the entity
 * @publicId: The public ID of the entity
 * @systemId: The system ID of the entity
 * @notationName: the name of the notation
 *
 * What to do when an unparsed entity declaration is parsed
 */
void
xmlSAX2UnparsedEntityDecl(void *ctx, const xmlChar *name,
		   const xmlChar *publicId, const xmlChar *systemId,
		   const xmlChar *notationName)
{
    xmlEntityPtr ent;
    xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr) ctx;
    if (ctx == NULL) return;
    if (ctxt->inSubset == 1) {
	ent = xmlAddDocEntity(ctxt->myDoc, name,
			XML_EXTERNAL_GENERAL_UNPARSED_ENTITY,
			publicId, systemId, notationName);
	if ((ent == NULL) && (ctxt->pedantic) &&
	    (ctxt->sax != NULL) && (ctxt->sax->warning != NULL))
	    ctxt->sax->warning(ctxt->userData, 
	     "Entity(%s) already defined in the internal subset\n", name);
	if ((ent != NULL) && (ent->URI == NULL) && (systemId != NULL)) {
	    xmlChar *URI;
	    const char *base = NULL;

	    if (ctxt->input != NULL)
		base = ctxt->input->filename;
	    if (base == NULL)
		base = ctxt->directory;
	
	    URI = xmlBuildURI(systemId, (const xmlChar *) base);
	    ent->URI = URI;
	}
    } else if (ctxt->inSubset == 2) {
	ent = xmlAddDtdEntity(ctxt->myDoc, name,
			XML_EXTERNAL_GENERAL_UNPARSED_ENTITY,
			publicId, systemId, notationName);
	if ((ent == NULL) && (ctxt->pedantic) &&
	    (ctxt->sax != NULL) && (ctxt->sax->warning != NULL))
	    ctxt->sax->warning(ctxt->userData, 
	     "Entity(%s) already defined in the external subset\n", name);
	if ((ent != NULL) && (ent->URI == NULL) && (systemId != NULL)) {
	    xmlChar *URI;
	    const char *base = NULL;

	    if (ctxt->input != NULL)
		base = ctxt->input->filename;
	    if (base == NULL)
		base = ctxt->directory;
	
	    URI = xmlBuildURI(systemId, (const xmlChar *) base);
	    ent->URI = URI;
	}
    } else {
        SAXxmlFatalErrMsg(ctxt, XML_ERR_INTERNAL_ERROR,
	     "SAX.xmlSAX2UnparsedEntityDecl(%s) called while not in subset\n",
	               name, NULL);
    }
}

/**
 * xmlSAX2SetDocumentLocator:
 * @ctx: the user data (XML parser context)
 * @loc: A SAX Locator
 *
 * Receive the document locator at startup, actually xmlDefaultSAXLocator
 * Everything is available on the context, so this is useless in our case.
 */
void
xmlSAX2SetDocumentLocator(void *ctx ATTRIBUTE_UNUSED, xmlSAXLocatorPtr loc ATTRIBUTE_UNUSED)
{
    /* xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr) ctx; */
}

/**
 * xmlSAX2StartDocument:
 * @ctx: the user data (XML parser context)
 *
 * called when the document start being processed.
 */
void
xmlSAX2StartDocument(void *ctx)
{
    xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr) ctx;
    xmlDocPtr doc;

    if (ctx == NULL) return;

    if (ctxt->html) {
        xmlGenericError(xmlGenericErrorContext,
		"libxml2 built without HTML support\n");
	ctxt->errNo = XML_ERR_INTERNAL_ERROR;
	ctxt->instate = XML_PARSER_EOF;
	ctxt->disableSAX = 1;
	return;
    } else {
	doc = ctxt->myDoc = xmlNewDoc(ctxt->version);
	if (doc != NULL) {
	    doc->properties = 0;
	    if (ctxt->options & XML_PARSE_OLD10)
	        doc->properties |= XML_DOC_OLD10;
	    doc->parseFlags = ctxt->options;
	    if (ctxt->encoding != NULL)
		doc->encoding = xmlStrdup(ctxt->encoding);
	    else
		doc->encoding = NULL;
	    doc->standalone = ctxt->standalone;
	} else {
	    xmlSAX2ErrMemory(ctxt, "xmlSAX2StartDocument");
	    return;
	}
	if ((ctxt->dictNames) && (doc != NULL)) {
	    doc->dict = ctxt->dict;
	    xmlDictReference(doc->dict);
	}
    }
    if ((ctxt->myDoc != NULL) && (ctxt->myDoc->URL == NULL) &&
	(ctxt->input != NULL) && (ctxt->input->filename != NULL)) {
	ctxt->myDoc->URL = xmlPathToURI((const xmlChar *)ctxt->input->filename);
	if (ctxt->myDoc->URL == NULL)
	    xmlSAX2ErrMemory(ctxt, "xmlSAX2StartDocument");
    }
}

/**
 * xmlSAX2EndDocument:
 * @ctx: the user data (XML parser context)
 *
 * called when the document end has been detected.
 */
void
xmlSAX2EndDocument(void *ctx)
{
    xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr) ctx;
    if (ctx == NULL) return;

    /*
     * Grab the encoding if it was added on-the-fly
     */
    if ((ctxt->encoding != NULL) && (ctxt->myDoc != NULL) &&
	(ctxt->myDoc->encoding == NULL)) {
	ctxt->myDoc->encoding = ctxt->encoding;
	ctxt->encoding = NULL;
    }
    if ((ctxt->inputTab != NULL) &&
        (ctxt->inputNr > 0) && (ctxt->inputTab[0] != NULL) &&
        (ctxt->inputTab[0]->encoding != NULL) && (ctxt->myDoc != NULL) &&
	(ctxt->myDoc->encoding == NULL)) {
	ctxt->myDoc->encoding = xmlStrdup(ctxt->inputTab[0]->encoding);
    }
    if ((ctxt->charset != XML_CHAR_ENCODING_NONE) && (ctxt->myDoc != NULL) &&
	(ctxt->myDoc->charset == XML_CHAR_ENCODING_NONE)) {
	ctxt->myDoc->charset = ctxt->charset;
    }
}

/*
 * xmlSAX2TextNode:
 * @ctxt:  the parser context
 * @str:  the input string
 * @len: the string length
 * 
 * Remove the entities from an attribute value
 *
 * Returns the newly allocated string or NULL if not needed or error
 */
static xmlNodePtr
xmlSAX2TextNode(xmlParserCtxtPtr ctxt, const xmlChar *str, int len) {
    xmlNodePtr ret;
    const xmlChar *intern = NULL;

    /*
     * Allocate
     */
    if (ctxt->freeElems != NULL) {
	ret = ctxt->freeElems;
	ctxt->freeElems = ret->next;
	ctxt->freeElemsNr--;
    } else {
	ret = (xmlNodePtr) xmlMalloc(sizeof(xmlNode));
    }
    if (ret == NULL) {
        xmlErrMemory(ctxt, "xmlSAX2Characters");
	return(NULL);
    }
    memset(ret, 0, sizeof(xmlNode));
    /*
     * intern the formatting blanks found between tags, or the
     * very short strings
     */
    if (ctxt->dictNames) {
        xmlChar cur = str[len];

	if ((len < (int) (2 * sizeof(void *))) &&
	    (ctxt->options & XML_PARSE_COMPACT)) {
	    /* store the string in the node overrithing properties and nsDef */
	    xmlChar *tmp = (xmlChar *) &(ret->properties);
	    memcpy(tmp, str, len);
	    tmp[len] = 0;
	    intern = tmp;
	} else if ((len <= 3) && ((cur == '"') || (cur == '\'') ||
	    ((cur == '<') && (str[len + 1] != '!')))) {
	    intern = xmlDictLookup(ctxt->dict, str, len);
	} else if (IS_BLANK_CH(*str) && (len < 60) && (cur == '<') &&
	           (str[len + 1] != '!')) {
	    int i;

	    for (i = 1;i < len;i++) {
		if (!IS_BLANK_CH(str[i])) goto skip;
	    }
	    intern = xmlDictLookup(ctxt->dict, str, len);
	}
    }
skip:
    ret->type = XML_TEXT_NODE;

    ret->name = xmlStringText;
    if (intern == NULL) {
	ret->content = xmlStrndup(str, len);
	if (ret->content == NULL) {
	    xmlSAX2ErrMemory(ctxt, "xmlSAX2TextNode");
	    xmlFree(ret);
	    return(NULL);
	}
    } else
	ret->content = (xmlChar *) intern;

    if (ctxt->input != NULL)
        ret->line = ctxt->input->line;

    if ((__xmlRegisterCallbacks) && (xmlRegisterNodeDefaultValue))
	xmlRegisterNodeDefaultValue(ret);
    return(ret);
}

/**
 * xmlSAX2AttributeNs:
 * @ctx: the user data (XML parser context)
 * @localname:  the local name of the attribute
 * @prefix:  the attribute namespace prefix if available
 * @URI:  the attribute namespace name if available
 * @value:  Start of the attribute value
 * @valueend: end of the attribute value
 *
 * Handle an attribute that has been read by the parser.
 * The default handling is to convert the attribute into an
 * DOM subtree and past it in a new xmlAttr element added to
 * the element.
 */
static void
xmlSAX2AttributeNs(xmlParserCtxtPtr ctxt,
                   const xmlChar * localname,
                   const xmlChar * prefix,
		   const xmlChar * value,
		   const xmlChar * valueend)
{
    xmlAttrPtr ret;
    xmlNsPtr namespace = NULL;
    xmlChar *dup = NULL;

    /*
     * Note: if prefix == NULL, the attribute is not in the default namespace
     */
    if (prefix != NULL)
	namespace = xmlSearchNs(ctxt->myDoc, ctxt->node, prefix);

    /*
     * allocate the node
     */
    if (ctxt->freeAttrs != NULL) {
        ret = ctxt->freeAttrs;
	ctxt->freeAttrs = ret->next;
	ctxt->freeAttrsNr--;
	memset(ret, 0, sizeof(xmlAttr));
	ret->type = XML_ATTRIBUTE_NODE;

	ret->parent = ctxt->node; 
	ret->doc = ctxt->myDoc;
	ret->ns = namespace;

	if (ctxt->dictNames)
	    ret->name = localname;
	else
	    ret->name = xmlStrdup(localname);

        /* link at the end to preserv order, TODO speed up with a last */
	if (ctxt->node->properties == NULL) {
	    ctxt->node->properties = ret;
	} else {
	    xmlAttrPtr prev = ctxt->node->properties;

	    while (prev->next != NULL) prev = prev->next;
	    prev->next = ret;
	    ret->prev = prev;
	}

	if ((__xmlRegisterCallbacks) && (xmlRegisterNodeDefaultValue))
	    xmlRegisterNodeDefaultValue((xmlNodePtr)ret);
    } else {
	if (ctxt->dictNames)
	    ret = xmlNewNsPropEatName(ctxt->node, namespace, 
	                              (xmlChar *) localname, NULL);
	else
	    ret = xmlNewNsProp(ctxt->node, namespace, localname, NULL);
	if (ret == NULL) {
	    xmlErrMemory(ctxt, "xmlSAX2AttributeNs");
	    return;
	}
    }

    if ((ctxt->replaceEntities == 0) && (!ctxt->html)) {
	xmlNodePtr tmp;

	/*
	 * We know that if there is an entity reference, then
	 * the string has been dup'ed and terminates with 0
	 * otherwise with ' or "
	 */
	if (*valueend != 0) {
	    tmp = xmlSAX2TextNode(ctxt, value, valueend - value);
	    ret->children = tmp;
	    ret->last = tmp;
	    if (tmp != NULL) {
		tmp->doc = ret->doc;
		tmp->parent = (xmlNodePtr) ret;
	    }
	} else {
	    ret->children = xmlStringLenGetNodeList(ctxt->myDoc, value,
						    valueend - value);
	    tmp = ret->children;
	    while (tmp != NULL) {
	        tmp->doc = ret->doc;
		tmp->parent = (xmlNodePtr) ret;
		if (tmp->next == NULL)
		    ret->last = tmp;
		tmp = tmp->next;
	    }
	}
    } else if (value != NULL) {
	xmlNodePtr tmp;

	tmp = xmlSAX2TextNode(ctxt, value, valueend - value);
	ret->children = tmp;
	ret->last = tmp;
	if (tmp != NULL) {
	    tmp->doc = ret->doc;
	    tmp->parent = (xmlNodePtr) ret;
	}
    }

           if (((ctxt->loadsubset & XML_SKIP_IDS) == 0) &&
	       (((ctxt->replaceEntities == 0) && (ctxt->external != 2)) ||
	        ((ctxt->replaceEntities != 0) && (ctxt->inSubset == 0)))) {
        /*
	 * when validating, the ID registration is done at the attribute
	 * validation level. Otherwise we have to do specific handling here.
	 */
        if ((prefix == ctxt->str_xml) &&
	           (localname[0] == 'i') && (localname[1] == 'd') &&
		   (localname[2] == 0)) {
	    /*
	     * Add the xml:id value
	     *
	     * Open issue: normalization of the value.
	     */
	    if (dup == NULL)
	        dup = xmlStrndup(value, valueend - value);
	    xmlAddID(&ctxt->vctxt, ctxt->myDoc, dup, ret);
	} else if (xmlIsID(ctxt->myDoc, ctxt->node, ret)) {
	    /* might be worth duplicate entry points and not copy */
	    if (dup == NULL)
	        dup = xmlStrndup(value, valueend - value);
	    xmlAddID(&ctxt->vctxt, ctxt->myDoc, dup, ret);
	} else if (xmlIsRef(ctxt->myDoc, ctxt->node, ret)) {
	    if (dup == NULL)
	        dup = xmlStrndup(value, valueend - value);
	    xmlAddRef(&ctxt->vctxt, ctxt->myDoc, dup, ret);
	}
    }
    if (dup != NULL)
	xmlFree(dup);
}

/**
 * xmlSAX2StartElementNs:
 * @ctx:  the user data (XML parser context)
 * @localname:  the local name of the element
 * @prefix:  the element namespace prefix if available
 * @URI:  the element namespace name if available
 * @nb_namespaces:  number of namespace definitions on that node
 * @namespaces:  pointer to the array of prefix/URI pairs namespace definitions
 * @nb_attributes:  the number of attributes on that node
 * @nb_defaulted:  the number of defaulted attributes.
 * @attributes:  pointer to the array of (localname/prefix/URI/value/end)
 *               attribute values.
 *
 * SAX2 callback when an element start has been detected by the parser.
 * It provides the namespace informations for the element, as well as
 * the new namespace declarations on the element.
 */
void
xmlSAX2StartElementNs(void *ctx,
                      const xmlChar *localname,
		      const xmlChar *prefix,
		      const xmlChar *URI,
		      int nb_namespaces,
		      const xmlChar **namespaces,
		      int nb_attributes,
		      int nb_defaulted,
		      const xmlChar **attributes)
{
    xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr) ctx;
    xmlNodePtr ret;
    xmlNodePtr parent;
    xmlNsPtr last = NULL, ns;
    const xmlChar *uri, *pref;
    xmlChar *lname = NULL;
    int i, j;

    if (ctx == NULL) return;
    parent = ctxt->node;
    /*
     * First check on validity:
     */
    if (ctxt->validate && (ctxt->myDoc->extSubset == NULL) && 
        ((ctxt->myDoc->intSubset == NULL) ||
	 ((ctxt->myDoc->intSubset->notations == NULL) && 
	  (ctxt->myDoc->intSubset->elements == NULL) &&
	  (ctxt->myDoc->intSubset->attributes == NULL) && 
	  (ctxt->myDoc->intSubset->entities == NULL)))) {
	SAXxmlErrValid(ctxt, XML_ERR_NO_DTD,
	  "Validation failed: no DTD found !", NULL, NULL);
	ctxt->validate = 0;
    }

    /*
     * Take care of the rare case of an undefined namespace prefix
     */
    if ((prefix != NULL) && (URI == NULL)) {
        if (ctxt->dictNames) {
	    const xmlChar *fullname;

	    fullname = xmlDictQLookup(ctxt->dict, prefix, localname);
	    if (fullname != NULL)
	        localname = fullname;
	} else {
	    lname = xmlBuildQName(localname, prefix, NULL, 0);
	}
    }
    /*
     * allocate the node
     */
    if (ctxt->freeElems != NULL) {
        ret = ctxt->freeElems;
	ctxt->freeElems = ret->next;
	ctxt->freeElemsNr--;
	memset(ret, 0, sizeof(xmlNode));
	ret->type = XML_ELEMENT_NODE;

	if (ctxt->dictNames)
	    ret->name = localname;
	else {
	    if (lname == NULL)
		ret->name = xmlStrdup(localname);
	    else
	        ret->name = lname;
	    if (ret->name == NULL) {
	        xmlSAX2ErrMemory(ctxt, "xmlSAX2StartElementNs");
		return;
	    }
	}
	if ((__xmlRegisterCallbacks) && (xmlRegisterNodeDefaultValue))
	    xmlRegisterNodeDefaultValue(ret);
    } else {
	if (ctxt->dictNames)
	    ret = xmlNewDocNodeEatName(ctxt->myDoc, NULL, 
	                               (xmlChar *) localname, NULL);
	else if (lname == NULL)
	    ret = xmlNewDocNode(ctxt->myDoc, NULL, localname, NULL);
	else
	    ret = xmlNewDocNodeEatName(ctxt->myDoc, NULL, 
	                               (xmlChar *) lname, NULL);
	if (ret == NULL) {
	    xmlSAX2ErrMemory(ctxt, "xmlSAX2StartElementNs");
	    return;
	}
    }
    if (ctxt->linenumbers) {
	if (ctxt->input != NULL) {
	    if (ctxt->input->line < 65535)
		ret->line = (short) ctxt->input->line;
	    else
	        ret->line = 65535;
	}
    }

    if ((ctxt->myDoc->children == NULL) || (parent == NULL)) {
        xmlAddChild((xmlNodePtr) ctxt->myDoc, (xmlNodePtr) ret);
    }
    /*
     * Build the namespace list
     */
    for (i = 0,j = 0;j < nb_namespaces;j++) {
        pref = namespaces[i++];
	uri = namespaces[i++];
	ns = xmlNewNs(NULL, uri, pref);
	if (ns != NULL) {
	    if (last == NULL) {
	        ret->nsDef = last = ns;
	    } else {
	        last->next = ns;
		last = ns;
	    }
	    if ((URI != NULL) && (prefix == pref))
		ret->ns = ns;
	} else {
            /*
             * any out of memory error would already have been raised
             * but we can't be garanteed it's the actual error due to the
             * API, best is to skip in this case
             */
	    continue;
	}
    }
    ctxt->nodemem = -1;

    /*
     * We are parsing a new node.
     */
    nodePush(ctxt, ret);

    /*
     * Link the child element
     */
    if (parent != NULL) {
        if (parent->type == XML_ELEMENT_NODE) {
	    xmlAddChild(parent, ret);
	} else {
	    xmlAddSibling(parent, ret);
	}
    }

    /*
     * Insert the defaulted attributes from the DTD only if requested:
     */
    if ((nb_defaulted != 0) &&
        ((ctxt->loadsubset & XML_COMPLETE_ATTRS) == 0))
	nb_attributes -= nb_defaulted;

    /*
     * Search the namespace if it wasn't already found
     * Note that, if prefix is NULL, this searches for the default Ns
     */
    if ((URI != NULL) && (ret->ns == NULL)) {
        ret->ns = xmlSearchNs(ctxt->myDoc, parent, prefix);
	if ((ret->ns == NULL) && (xmlStrEqual(prefix, BAD_CAST "xml"))) {
	    ret->ns = xmlSearchNs(ctxt->myDoc, ret, prefix);
	}
	if (ret->ns == NULL) {
	    ns = xmlNewNs(ret, NULL, prefix);
	    if (ns == NULL) {

	        xmlSAX2ErrMemory(ctxt, "xmlSAX2StartElementNs");
		return;
	    }
            if (prefix != NULL)
                xmlNsWarnMsg(ctxt, XML_NS_ERR_UNDEFINED_NAMESPACE,
                             "Namespace prefix %s was not found\n",
                             prefix, NULL);
            else
                xmlNsWarnMsg(ctxt, XML_NS_ERR_UNDEFINED_NAMESPACE,
                             "Namespace default prefix was not found\n",
                             NULL, NULL);
	}
    }

    /*
     * process all the other attributes
     */
    if (nb_attributes > 0) {
        for (j = 0,i = 0;i < nb_attributes;i++,j+=5) {
	    /*
	     * Handle the rare case of an undefined atribute prefix
	     */
	    if ((attributes[j+1] != NULL) && (attributes[j+2] == NULL)) {
		if (ctxt->dictNames) {
		    const xmlChar *fullname;

		    fullname = xmlDictQLookup(ctxt->dict, attributes[j+1],
		                              attributes[j]);
		    if (fullname != NULL) {
			xmlSAX2AttributeNs(ctxt, fullname, NULL,
			                   attributes[j+3], attributes[j+4]);
		        continue;
		    }
		} else {
		    lname = xmlBuildQName(attributes[j], attributes[j+1],
		                          NULL, 0);
		    if (lname != NULL) {
			xmlSAX2AttributeNs(ctxt, lname, NULL,
			                   attributes[j+3], attributes[j+4]);
			xmlFree(lname);
		        continue;
		    }
		}
	    }
	    xmlSAX2AttributeNs(ctxt, attributes[j], attributes[j+1],
			       attributes[j+3], attributes[j+4]);
	}
    }

}

/**
 * xmlSAX2EndElementNs:
 * @ctx:  the user data (XML parser context)
 * @localname:  the local name of the element
 * @prefix:  the element namespace prefix if available
 * @URI:  the element namespace name if available
 *
 * SAX2 callback when an element end has been detected by the parser.
 * It provides the namespace informations for the element.
 */
void
xmlSAX2EndElementNs(void *ctx,
                    const xmlChar * localname ATTRIBUTE_UNUSED,
                    const xmlChar * prefix ATTRIBUTE_UNUSED,
		    const xmlChar * URI ATTRIBUTE_UNUSED)
{
    xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr) ctx;
    xmlParserNodeInfo node_info;
    xmlNodePtr cur;

    if (ctx == NULL) return;
    cur = ctxt->node;
    /* Capture end position and add node */
    if ((ctxt->record_info) && (cur != NULL)) {
        node_info.end_pos = ctxt->input->cur - ctxt->input->base;
        node_info.end_line = ctxt->input->line;
        node_info.node = cur;
        xmlParserAddNodeInfo(ctxt, &node_info);
    }
    ctxt->nodemem = -1;


    /*
     * end of parsing of this node.
     */
    nodePop(ctxt);
}

/**
 * xmlSAX2Reference:
 * @ctx: the user data (XML parser context)
 * @name:  The entity name
 *
 * called when an entity xmlSAX2Reference is detected. 
 */
void
xmlSAX2Reference(void *ctx, const xmlChar *name)
{
    xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr) ctx;
    xmlNodePtr ret;

    if (ctx == NULL) return;
    if (name[0] == '#')
	ret = xmlNewCharRef(ctxt->myDoc, name);
    else
	ret = xmlNewReference(ctxt->myDoc, name);
    if (xmlAddChild(ctxt->node, ret) == NULL) {
        xmlFreeNode(ret);
    }
}

/**
 * xmlSAX2Characters:
 * @ctx: the user data (XML parser context)
 * @ch:  a xmlChar string
 * @len: the number of xmlChar
 *
 * receiving some chars from the parser.
 */
void
xmlSAX2Characters(void *ctx, const xmlChar *ch, int len)
{
    xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr) ctx;
    xmlNodePtr lastChild;

    if (ctx == NULL) return;
    /*
     * Handle the data if any. If there is no child
     * add it as content, otherwise if the last child is text,
     * concatenate it, else create a new node of type text.
     */

    if (ctxt->node == NULL) {
        return;
    }
    lastChild = ctxt->node->last;

    /*
     * Here we needed an accelerator mechanism in case of very large
     * elements. Use an attribute in the structure !!!
     */
    if (lastChild == NULL) {
        lastChild = xmlSAX2TextNode(ctxt, ch, len);
	if (lastChild != NULL) {
	    ctxt->node->children = lastChild;
	    ctxt->node->last = lastChild;
	    lastChild->parent = ctxt->node;
	    lastChild->doc = ctxt->node->doc;
	    ctxt->nodelen = len;
	    ctxt->nodemem = len + 1;
	} else {
	    xmlSAX2ErrMemory(ctxt, "xmlSAX2Characters");
	    return;
	}
    } else {
	int coalesceText = (lastChild != NULL) &&
	    (lastChild->type == XML_TEXT_NODE) &&
	    (lastChild->name == xmlStringText);
	if ((coalesceText) && (ctxt->nodemem != 0)) {
	    /*
	     * The whole point of maintaining nodelen and nodemem,
	     * xmlTextConcat is too costly, i.e. compute length,
	     * reallocate a new buffer, move data, append ch. Here
	     * We try to minimaze realloc() uses and avoid copying
	     * and recomputing length over and over.
	     */
	    if (lastChild->content == (xmlChar *)&(lastChild->properties)) {
		lastChild->content = xmlStrdup(lastChild->content);
		lastChild->properties = NULL;
	    } else if ((ctxt->nodemem == ctxt->nodelen + 1) &&
	               (xmlDictOwns(ctxt->dict, lastChild->content))) {
		lastChild->content = xmlStrdup(lastChild->content);
	    }
            if (((size_t)ctxt->nodelen + (size_t)len > XML_MAX_TEXT_LENGTH) &&
                ((ctxt->options & XML_PARSE_HUGE) == 0)) {
                xmlSAX2ErrMemory(ctxt, "xmlSAX2Characters: huge text node");
                return;
            }
	    if ((size_t)ctxt->nodelen > SIZE_T_MAX - (size_t)len || 
	        (size_t)ctxt->nodemem + (size_t)len > SIZE_T_MAX / 2) {
                xmlSAX2ErrMemory(ctxt, "xmlSAX2Characters overflow prevented");
                return;
	    }
	    if (ctxt->nodelen + len >= ctxt->nodemem) {
		xmlChar *newbuf;
		size_t size;

		size = ctxt->nodemem + len;
		size *= 2;
                newbuf = (xmlChar *) xmlRealloc(lastChild->content,size);
		if (newbuf == NULL) {
		    xmlSAX2ErrMemory(ctxt, "xmlSAX2Characters");
		    return;
		}
		ctxt->nodemem = size;
		lastChild->content = newbuf;
	    }
	    memcpy(&lastChild->content[ctxt->nodelen], ch, len);
	    ctxt->nodelen += len;
	    lastChild->content[ctxt->nodelen] = 0;
	} else if (coalesceText) {
	    if (xmlTextConcat(lastChild, ch, len)) {
		xmlSAX2ErrMemory(ctxt, "xmlSAX2Characters");
	    }
	    if (ctxt->node->children != NULL) {
		ctxt->nodelen = xmlStrlen(lastChild->content);
		ctxt->nodemem = ctxt->nodelen + 1;
	    }
	} else {
	    /* Mixed content, first time */
	    lastChild = xmlSAX2TextNode(ctxt, ch, len);
	    if (lastChild != NULL) {
		xmlAddChild(ctxt->node, lastChild);
		if (ctxt->node->children != NULL) {
		    ctxt->nodelen = len;
		    ctxt->nodemem = len + 1;
		}
	    }
	}
    }
}

/**
 * xmlSAX2IgnorableWhitespace:
 * @ctx: the user data (XML parser context)
 * @ch:  a xmlChar string
 * @len: the number of xmlChar
 *
 * receiving some ignorable whitespaces from the parser.
 * UNUSED: by default the DOM building will use xmlSAX2Characters
 */
void
xmlSAX2IgnorableWhitespace(void *ctx ATTRIBUTE_UNUSED, const xmlChar *ch ATTRIBUTE_UNUSED, int len ATTRIBUTE_UNUSED)
{
    /* xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr) ctx; */
}

/**
 * xmlSAX2ProcessingInstruction:
 * @ctx: the user data (XML parser context)
 * @target:  the target name
 * @data: the PI data's
 *
 * A processing instruction has been parsed.
 */
void
xmlSAX2ProcessingInstruction(void *ctx, const xmlChar *target,
                      const xmlChar *data)
{
    xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr) ctx;
    xmlNodePtr ret;
    xmlNodePtr parent;

    if (ctx == NULL) return;
    parent = ctxt->node;

    ret = xmlNewDocPI(ctxt->myDoc, target, data);
    if (ret == NULL) return;

    if (ctxt->linenumbers) {
	if (ctxt->input != NULL) {
	    if (ctxt->input->line < 65535)
		ret->line = (short) ctxt->input->line;
	    else
	        ret->line = 65535;
	}
    }
    if (ctxt->inSubset == 1) {
	xmlAddChild((xmlNodePtr) ctxt->myDoc->intSubset, ret);
	return;
    } else if (ctxt->inSubset == 2) {
	xmlAddChild((xmlNodePtr) ctxt->myDoc->extSubset, ret);
	return;
    }
    if ((ctxt->myDoc->children == NULL) || (parent == NULL)) {
        xmlAddChild((xmlNodePtr) ctxt->myDoc, (xmlNodePtr) ret);
	return;
    }
    if (parent->type == XML_ELEMENT_NODE) {
	xmlAddChild(parent, ret);
    } else {
	xmlAddSibling(parent, ret);
    }
}

/**
 * xmlSAX2Comment:
 * @ctx: the user data (XML parser context)
 * @value:  the xmlSAX2Comment content
 *
 * A xmlSAX2Comment has been parsed.
 */
void
xmlSAX2Comment(void *ctx, const xmlChar *value)
{
    xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr) ctx;
    xmlNodePtr ret;
    xmlNodePtr parent;

    if (ctx == NULL) return;
    parent = ctxt->node;
    ret = xmlNewDocComment(ctxt->myDoc, value);
    if (ret == NULL) return;
    if (ctxt->linenumbers) {
	if (ctxt->input != NULL) {
	    if (ctxt->input->line < 65535)
		ret->line = (short) ctxt->input->line;
	    else
	        ret->line = 65535;
	}
    }

    if (ctxt->inSubset == 1) {
	xmlAddChild((xmlNodePtr) ctxt->myDoc->intSubset, ret);
	return;
    } else if (ctxt->inSubset == 2) {
	xmlAddChild((xmlNodePtr) ctxt->myDoc->extSubset, ret);
	return;
    }
    if ((ctxt->myDoc->children == NULL) || (parent == NULL)) {
        xmlAddChild((xmlNodePtr) ctxt->myDoc, (xmlNodePtr) ret);
	return;
    }
    if (parent->type == XML_ELEMENT_NODE) {
	xmlAddChild(parent, ret);
    } else {
	xmlAddSibling(parent, ret);
    }
}

/**
 * xmlSAX2CDataBlock:
 * @ctx: the user data (XML parser context)
 * @value:  The pcdata content
 * @len:  the block length
 *
 * called when a pcdata block has been parsed
 */
void
xmlSAX2CDataBlock(void *ctx, const xmlChar *value, int len)
{
    xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr) ctx;
    xmlNodePtr ret, lastChild;

    if (ctx == NULL) return;
    lastChild = xmlGetLastChild(ctxt->node);
    if ((lastChild != NULL) &&
        (lastChild->type == XML_CDATA_SECTION_NODE)) {
	xmlTextConcat(lastChild, value, len);
    } else {
	ret = xmlNewCDataBlock(ctxt->myDoc, value, len);
	xmlAddChild(ctxt->node, ret);
    }
}

static int xmlSAX2DefaultVersionValue = 2;

/**
 * xmlSAXVersion:
 * @hdlr:  the SAX handler
 * @version:  the version, 1 or 2
 *
 * Initialize the default XML SAX handler according to the version
 *
 * Returns 0 in case of success and -1 in case of error.
 */
int
xmlSAXVersion(xmlSAXHandler *hdlr, int version)
{
    if (hdlr == NULL) return(-1);
    if (version == 2) {
	hdlr->startElement = NULL;
	hdlr->endElement = NULL;
	hdlr->startElementNs = xmlSAX2StartElementNs;
	hdlr->endElementNs = xmlSAX2EndElementNs;
	hdlr->serror = NULL;
	hdlr->initialized = XML_SAX2_MAGIC;
    } else
        return(-1);
    hdlr->internalSubset = xmlSAX2InternalSubset;
    hdlr->externalSubset = xmlSAX2ExternalSubset;
    hdlr->isStandalone = xmlSAX2IsStandalone;
    hdlr->hasInternalSubset = xmlSAX2HasInternalSubset;
    hdlr->hasExternalSubset = xmlSAX2HasExternalSubset;
    hdlr->resolveEntity = xmlSAX2ResolveEntity;
    hdlr->getEntity = xmlSAX2GetEntity;
    hdlr->getParameterEntity = xmlSAX2GetParameterEntity;
    hdlr->entityDecl = xmlSAX2EntityDecl;
    hdlr->attributeDecl = xmlSAX2AttributeDecl;
    hdlr->elementDecl = xmlSAX2ElementDecl;
    hdlr->notationDecl = xmlSAX2NotationDecl;
    hdlr->unparsedEntityDecl = xmlSAX2UnparsedEntityDecl;
    hdlr->setDocumentLocator = xmlSAX2SetDocumentLocator;
    hdlr->startDocument = xmlSAX2StartDocument;
    hdlr->endDocument = xmlSAX2EndDocument;
    hdlr->reference = xmlSAX2Reference;
    hdlr->characters = xmlSAX2Characters;
    hdlr->cdataBlock = xmlSAX2CDataBlock;
    hdlr->ignorableWhitespace = xmlSAX2Characters;
    hdlr->processingInstruction = xmlSAX2ProcessingInstruction;
    hdlr->comment = xmlSAX2Comment;
    hdlr->warning = xmlParserWarning;
    hdlr->error = xmlParserError;
    hdlr->fatalError = xmlParserError;

    return(0);
}

/**
 * xmlSAX2InitDefaultSAXHandler:
 * @hdlr:  the SAX handler
 * @warning:  flag if non-zero sets the handler warning procedure
 *
 * Initialize the default XML SAX2 handler
 */
void
xmlSAX2InitDefaultSAXHandler(xmlSAXHandler *hdlr, int warning)
{
    if ((hdlr == NULL) || (hdlr->initialized != 0))
	return;

    xmlSAXVersion(hdlr, xmlSAX2DefaultVersionValue);
    if (warning == 0)
	hdlr->warning = NULL;
    else
	hdlr->warning = xmlParserWarning;
}

/**
 * xmlDefaultSAXHandlerInit:
 *
 * Initialize the default SAX2 handler
 */
void
xmlDefaultSAXHandlerInit(void)
{
}

#define bottom_SAX2
#include "elfgcchack.h"
