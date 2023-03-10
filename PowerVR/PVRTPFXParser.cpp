/******************************************************************************

 @File         PVRTPFXParser.cpp

 @Title        PVRTPFXParser

 @Version      

 @Copyright    Copyright (c) Imagination Technologies Limited.

 @Platform     Windows + Linux

 @Description  PFX file parser.

******************************************************************************/

/*****************************************************************************
** Includes
******************************************************************************/
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "PVRTGlobal.h"
//#include "PVRTContext.h"
#include "PVRTMatrix.h"
#include "PVRTFixedPoint.h"
#include "PVRTMisc.h"
#include "PVRTPFXParser.h"
#include "PVRTResourceFile.h"
#include "PVRTString.h"
#include "PVRTMisc.h"		// Used for POT functions

/****************************************************************************
** Constants
****************************************************************************/
const char* c_pszLinear   = "LINEAR";
const char* c_pszNearest  = "NEAREST";
const char* c_pszNone	  = "NONE";
const char* c_pszClamp    = "CLAMP";
const char* c_pszRepeat	  = "REPEAT";

const char* c_ppszFilters[eFilter_Size] = 
{ 
	c_pszNearest,		// eFilter_Nearest
	c_pszLinear,		// eFilter_Linear
	c_pszNone,			// eFilter_None
};
const char* c_ppszWraps[eWrap_Size] = 
{ 
	c_pszClamp,			// eWrap_Clamp
	c_pszRepeat			// eWrap_Repeat
};

/****************************************************************************
** CPVRTPFXParserReadContext Class
****************************************************************************/
class CPVRTPFXParserReadContext
{
public:
	char			**ppszEffectFile;
	int				*pnFileLineNumber;
	unsigned int	nNumLines, nMaxLines;

public:
	CPVRTPFXParserReadContext();
	~CPVRTPFXParserReadContext();
};

/*!***************************************************************************
 @Function			CPVRTPFXParserReadContext
 @Description		Initialises values.
*****************************************************************************/
CPVRTPFXParserReadContext::CPVRTPFXParserReadContext()
{
	nMaxLines = 5000;
	nNumLines = 0;
	ppszEffectFile		= new char*[nMaxLines];
	pnFileLineNumber	= new int[nMaxLines];
}

/*!***************************************************************************
 @Function			~CPVRTPFXParserReadContext
 @Description		Frees allocated memory
*****************************************************************************/
CPVRTPFXParserReadContext::~CPVRTPFXParserReadContext()
{
	// free effect file
	for(unsigned int i = 0; i < nNumLines; i++)
	{
		FREE(ppszEffectFile[i]);
	}
	delete [] ppszEffectFile;
	delete [] pnFileLineNumber;
}


/*!***************************************************************************
 @Function			IgnoreWhitespace
 @Input				pszString
 @Output			pszString
 @Description		Skips space, tab, new-line and return characters.
*****************************************************************************/
void IgnoreWhitespace(char **pszString)
{
	while(	*pszString[0] == '\t' ||
			*pszString[0] == '\n' ||
			*pszString[0] == '\r' ||
			*pszString[0] == ' ' )
	{
		(*pszString)++;
	}
}

/*!***************************************************************************
 @Function			ReadEOLToken
 @Input				pToken
 @Output			char*
 @Description		Reads next strings to the end of the line and interperts as
					a token.
*****************************************************************************/
char* ReadEOLToken(char* pToken)
{
	char* pReturn = NULL;

	char szDelim[2] = {'\n', 0};				// try newline
	pReturn = strtok(pToken, szDelim);			
	if(pReturn == NULL)
	{
		szDelim[0] = '\r';
		pReturn = strtok (pToken, szDelim);		// try linefeed
	}
	return pReturn;
}

/*!***************************************************************************
 @Function			GetSemanticDataFromString
 @Output			pDataItem
 @Modified			pszArgumentString
 @Input				eType
 @Output			pError				error message
 @Return			true if successful
 @Description		Extracts the semantic data from the string and stores it
					in the output SPVRTSemanticDefaultData parameter.
*****************************************************************************/
bool GetSemanticDataFromString(SPVRTSemanticDefaultData *pDataItem, const char * const pszArgumentString, ESemanticDefaultDataType eType, CPVRTString *pError)
{
	char *pszString = (char *)pszArgumentString;
	char *pszTmp;

	IgnoreWhitespace(&pszString);

	if(pszString[0] != '(')
	{
		*pError = CPVRTString("Missing '(' after ") + c_psSemanticDefaultDataTypeInfo[eType].pszName;
		return false;
	}
	pszString++;

	IgnoreWhitespace(&pszString);

	if(!strlen(pszString))
	{
		*pError = c_psSemanticDefaultDataTypeInfo[eType].pszName + CPVRTString(" missing arguments");
		return false;
	}

	pszTmp = pszString;
	switch(c_psSemanticDefaultDataTypeInfo[eType].eInternalType)
	{
		case eFloating:
			pDataItem->pfData[0] = (float)strtod(pszString, &pszTmp);
			break;
		case eInteger:
			pDataItem->pnData[0] = (int)strtol(pszString, &pszTmp, 10);
			break;
		case eBoolean:
			if(strncmp(pszString, "true", 4) == 0)
			{
				pDataItem->pbData[0] = true;
				pszTmp = &pszString[4];
			}
			else if(strncmp(pszString, "false", 5) == 0)
			{
				pDataItem->pbData[0] = false;
				pszTmp = &pszString[5];
			}
			break;
	}

	if(pszString == pszTmp)
	{
		size_t n = strcspn(pszString, ",\t ");
		char *pszError = (char *)malloc(n + 1);
		strcpy(pszError, "");
		strncat(pszError, pszString, n);
		*pError = CPVRTString("'") + pszError + "' unexpected for " + c_psSemanticDefaultDataTypeInfo[eType].pszName;
		FREE(pszError);
		return false;
	}
	pszString = pszTmp;

	IgnoreWhitespace(&pszString);

	for(unsigned int i = 1; i < c_psSemanticDefaultDataTypeInfo[eType].nNumberDataItems; i++)
	{
		if(!strlen(pszString))
		{
			*pError = c_psSemanticDefaultDataTypeInfo[eType].pszName + CPVRTString(" missing arguments");
			return false;
		}

		if(pszString[0] != ',')
		{
			size_t n = strcspn(pszString, ",\t ");
			char *pszError = (char *)malloc(n + 1);
			strcpy(pszError, "");
			strncat(pszError, pszString, n);
			*pError = CPVRTString("'") + pszError + "' unexpected for " + c_psSemanticDefaultDataTypeInfo[eType].pszName;
			FREE(pszError);
			return false;
		}
		pszString++;

		IgnoreWhitespace(&pszString);

		if(!strlen(pszString))
		{
			*pError = c_psSemanticDefaultDataTypeInfo[eType].pszName + CPVRTString(" missing arguments");
			return false;
		}

		pszTmp = pszString;
		switch(c_psSemanticDefaultDataTypeInfo[eType].eInternalType)
		{
			case eFloating:
				pDataItem->pfData[i] = (float)strtod(pszString, &pszTmp);
				break;
			case eInteger:
				pDataItem->pnData[i] = (int)strtol(pszString, &pszTmp, 10);
				break;
			case eBoolean:
				if(strncmp(pszString, "true", 4) == 0)
				{
					pDataItem->pbData[i] = true;
					pszTmp = &pszString[4];
				}
				else if(strncmp(pszString, "false", 5) == 0)
				{
					pDataItem->pbData[i] = false;
					pszTmp = &pszString[5];
				}
				break;
		}

		if(pszString == pszTmp)
		{
			size_t n = strcspn(pszString, ",\t ");
			char *pszError = (char *)malloc(n + 1);
			strcpy(pszError, "");
			strncat(pszError, pszString, n);
			*pError = CPVRTString("'") + pszError + "' unexpected for " + c_psSemanticDefaultDataTypeInfo[eType].pszName;
			FREE(pszError);
			return false;
		}
		pszString = pszTmp;

		IgnoreWhitespace(&pszString);
	}

	if(pszString[0] != ')')
	{
		size_t n = strcspn(pszString, "\t )");
		char *pszError = (char *)malloc(n + 1);
		strcpy(pszError, "");
		strncat(pszError, pszString, n);
		*pError = CPVRTString("'") + pszError + "' found when expecting ')' for " + c_psSemanticDefaultDataTypeInfo[eType].pszName;
		FREE(pszError);
		return false;
	}
	pszString++;

	IgnoreWhitespace(&pszString);

	if(strlen(pszString))
	{
		*pError = CPVRTString("'") + pszString + "' unexpected after ')'";
		return false;
	}

	return true;
}

/*!***************************************************************************
 @Function			ConcatenateLinesUntil
 @Output			pszOut		output text
 @Output			nLine		end line number
 @Input				nLine		start line number
 @Input				ppszLines	input text - one array element per line
 @Input				nLimit		number of lines input
 @Input				pszEnd		end string
 @Return			true if successful
 @Description		Outputs a block of text starting from nLine and ending
					when the string pszEnd is found.
*****************************************************************************/
static bool ConcatenateLinesUntil(char *&pszOut, int &nLine, const char * const * const ppszLines, const unsigned int nLimit, const char * const pszEnd)
{
	unsigned int	i, j;
	size_t			nLen;

	nLen = 0;
	for(i = nLine; i < nLimit; ++i)
	{
		if(strcmp(ppszLines[i], pszEnd) == 0)
			break;
		nLen += strlen(ppszLines[i]) + 1;
	}
	if(i == nLimit)
	{
		return false;
	}

	if(nLen)
	{
		++nLen;

		pszOut = (char*)malloc(nLen * sizeof(*pszOut));
		*pszOut = 0;

		for(j = nLine; j < i; ++j)
		{
			strcat(pszOut, ppszLines[j]);
			strcat(pszOut, "\n");
		}
	}
	else
	{
		pszOut = 0;
	}

	nLine = i;
	return true;
}

/*!***************************************************************************
 @Function			CPVRTPFXParser
 @Description		Sets initial values.
*****************************************************************************/
CPVRTPFXParser::CPVRTPFXParser()
{
	m_sHeader.pszVersion = NULL;
	m_sHeader.pszDescription = NULL;
	m_sHeader.pszCopyright = NULL;

	m_nMaxRenders = 1;		// Although more could be stored, Shaman only currently supports one additional render pass
	m_nNumRenderPasses = 0;
	m_psRenderPasses = new SPVRTPFXRenderPass[m_nMaxRenders];

	// NOTE: Temp hardcode viewport size
	m_uiViewportWidth = 640;
	m_uiViewportHeight = 480;
}

/*!***************************************************************************
 @Function			~CPVRTPFXParser
 @Description		Frees memory used.
*****************************************************************************/
CPVRTPFXParser::~CPVRTPFXParser()
{
	unsigned int i;

	// FREE header strings
	FREE(m_sHeader.pszVersion);
	FREE(m_sHeader.pszDescription);
	FREE(m_sHeader.pszCopyright);

	// Free render pass info
	for(i=0; i < m_nNumRenderPasses; ++i)
	{
		delete[] m_psRenderPasses[i].pszSemanticName;
		m_psRenderPasses[i].pszSemanticName = NULL;
		delete[] m_psRenderPasses[i].pszNodeName;
		m_psRenderPasses[i].pszNodeName = NULL;
	}
	delete [] m_psRenderPasses;

}

/*!***************************************************************************
 @Function			Parse
 @Output			pReturnError	error string
 @Return			bool			true for success parsing file
 @Description		Parses a loaded PFX file.
*****************************************************************************/
bool CPVRTPFXParser::Parse(CPVRTString * const pReturnError)
{
	int nEndLine = 0;
	int nHeaderCounter = 0, nTexturesCounter = 0;
	int i;
	unsigned int j,k;

	// Loop through the file
	for(unsigned int nLine=0; nLine < m_psContext->nNumLines; nLine++)
	{
		// Skip blank lines
		if(!*m_psContext->ppszEffectFile[nLine])
			continue;

		if(strcmp("[HEADER]", m_psContext->ppszEffectFile[nLine]) == 0)
		{
			if(nHeaderCounter>0)
			{
				*pReturnError = PVRTStringFromFormattedStr("[HEADER] redefined on line %d\n", m_psContext->pnFileLineNumber[nLine]);
				return false;
			}
			if(GetEndTag("HEADER", nLine, &nEndLine))
			{
				if(ParseHeader(nLine, nEndLine, pReturnError))
					nHeaderCounter++;
				else
					return false;
			}
			else
			{
				*pReturnError = PVRTStringFromFormattedStr("Missing [/HEADER] tag after [HEADER] on line %d\n", m_psContext->pnFileLineNumber[nLine]);
				return false;
			}
			nLine = nEndLine;
		}
		else if(strcmp("[TEXTURES]", m_psContext->ppszEffectFile[nLine]) == 0)
		{
			if(nTexturesCounter>0)
			{
				*pReturnError = PVRTStringFromFormattedStr("[TEXTURES] redefined on line %d\n", m_psContext->pnFileLineNumber[nLine]);
				return false;
			}
			if(GetEndTag("TEXTURES", nLine, &nEndLine))
			{
				if(ParseTextures(nLine, nEndLine, pReturnError))
					nTexturesCounter++;
				else
					return false;
			}
			else
			{
				*pReturnError = PVRTStringFromFormattedStr("Missing [/TEXTURES] tag after [TEXTURES] on line %d\n", m_psContext->pnFileLineNumber[nLine]);
				return false;
			}
			nLine = nEndLine;
		}
		else if(strcmp("[VERTEXSHADER]", m_psContext->ppszEffectFile[nLine]) == 0)
		{
			if(GetEndTag("VERTEXSHADER", nLine, &nEndLine))
			{
				SPVRTPFXParserShader VertexShader;
				if(ParseShader(nLine, nEndLine, pReturnError, VertexShader, "VERTEXSHADER"))
					m_psVertexShader.Append(VertexShader);
				else
					return false;
			}
			else
			{
				*pReturnError = PVRTStringFromFormattedStr("Missing [/VERTEXSHADER] tag after [VERTEXSHADER] on line %d\n", m_psContext->pnFileLineNumber[nLine]);
				return false;
			}
			nLine = nEndLine;
		}
		else if(strcmp("[FRAGMENTSHADER]", m_psContext->ppszEffectFile[nLine]) == 0)
		{
			if(GetEndTag("FRAGMENTSHADER", nLine, &nEndLine))
			{
				SPVRTPFXParserShader FragShader;
				if(ParseShader(nLine, nEndLine, pReturnError, FragShader, "FRAGMENTSHADER"))
					m_psFragmentShader.Append(FragShader);
				else
					return false;
			}
			else
			{
				*pReturnError = PVRTStringFromFormattedStr("Missing [/FRAGMENTSHADER] tag after [FRAGMENTSHADER] on line %d\n", m_psContext->pnFileLineNumber[nLine]);
				return false;
			}
			nLine = nEndLine;
		}
		else if(strcmp("[EFFECT]", m_psContext->ppszEffectFile[nLine]) == 0)
		{
			if(GetEndTag("EFFECT", nLine, &nEndLine))
			{
				SPVRTPFXParserEffect Effect;
				if(ParseEffect(Effect, nLine, nEndLine, pReturnError))
					m_psEffect.Append(Effect);
				else
					return false;
			}
			else
			{
				*pReturnError = PVRTStringFromFormattedStr("Missing [/EFFECT] tag after [EFFECT] on line %d\n", m_psContext->pnFileLineNumber[nLine]);
				return false;
			}
			nLine = nEndLine;
		}
		else
		{
			*pReturnError = PVRTStringFromFormattedStr("'%s' unexpected on line %d\n", m_psContext->ppszEffectFile[nLine], m_psContext->pnFileLineNumber[nLine]);
			return false;
		}
	}

	if(m_psEffect.GetSize() < 1)
	{
		*pReturnError = CPVRTString("No [EFFECT] found. PFX file must have at least one defined.\n");
		return false;
	}

	if(m_psFragmentShader.GetSize() < 1)
	{
		*pReturnError = CPVRTString("No [FRAGMENTSHADER] found. PFX file must have at least one defined.\n");;
		return false;
	}

	if(m_psVertexShader.GetSize() < 1)
	{
		*pReturnError = CPVRTString("No [VERTEXSHADER] found. PFX file must have at least one defined.\n");
		return false;
	}

	for(i = 0; i < m_psEffect.GetSize(); ++i)
	{
		for(j = 0; j < m_psEffect[i].nNumTextures; ++j)
		{
			unsigned int uiTexSize = m_psTexture.GetSize();
			for(k = 0; k < uiTexSize; ++k)
			{
				if(strcmp(m_psEffect[i].psTextures[j].pszName, m_psTexture[k].pszName) == 0)
					break;
			}

			if(!uiTexSize || k == uiTexSize)
			{
				*pReturnError = PVRTStringFromFormattedStr("Error: TEXTURE '%s' is not defined in [TEXTURES].\n", m_psEffect[i].psTextures[j].pszName);
				return false;
			}
		}
	}

	return true;
}

/*!***************************************************************************
 @Function			ParseFromMemory
 @Input				pszScript		PFX script
 @Output			pReturnError	error string
 @Return			EPVRTError		PVR_SUCCESS for success parsing file
									PVR_FAIL if file doesn't exist or is invalid
 @Description		Parses a PFX script from memory.
*****************************************************************************/
EPVRTError CPVRTPFXParser::ParseFromMemory(const char * const pszScript, CPVRTString * const pReturnError)
{
	CPVRTPFXParserReadContext	context;
	char			pszLine[512];
	const char		*pszEnd, *pszCurr;
	int				nLineCounter;
	unsigned int	nLen;
	unsigned int	nReduce;
	bool			bDone;

	if(!pszScript)
		return PVR_FAIL;

	m_psContext = &context;

	// Find & process each line
	nLineCounter	= 0;
	bDone			= false;
	pszCurr			= pszScript;
	while(!bDone)
	{
		nLineCounter++;

		while(*pszCurr == '\r')
			++pszCurr;

		// Find length of line
		pszEnd = strchr(pszCurr, '\n');
		if(pszEnd)
		{
			nLen = (unsigned int)(pszEnd - pszCurr);
		}
		else
		{
			nLen = (unsigned int)strlen(pszCurr);
			bDone = true;
		}

		nReduce = 0; // Tells how far to go back because of '\r'.
		while(nLen - nReduce > 0 && pszCurr[nLen - 1 - nReduce] == '\r')
			nReduce++;

		// Ensure pszLine will not be not overrun
		if(nLen+1-nReduce > sizeof(pszLine) / sizeof(*pszLine))
			nLen = sizeof(pszLine) / sizeof(*pszLine) - 1 + nReduce;

		// Copy line into pszLine
		strncpy(pszLine, pszCurr, nLen - nReduce);
		pszLine[nLen - nReduce] = 0;
		pszCurr += nLen + 1;

		_ASSERT(strchr(pszLine, '\r') == 0);
		_ASSERT(strchr(pszLine, '\n') == 0);

		// Ignore comments
		char *tmp = strstr(pszLine, "//");
		if(tmp != NULL)	*tmp = '\0';

		// Reduce whitespace to one character.
		ReduceWhitespace(pszLine);

		// Store the line, even if blank lines (to get correct errors from GLSL compiler).
		if(m_psContext->nNumLines < m_psContext->nMaxLines)
		{
			m_psContext->pnFileLineNumber[m_psContext->nNumLines] = nLineCounter;
			m_psContext->ppszEffectFile[m_psContext->nNumLines] = (char *)malloc((strlen(pszLine) + 1) * sizeof(char));
			strcpy(m_psContext->ppszEffectFile[m_psContext->nNumLines], pszLine);
			m_psContext->nNumLines++;
		}
		else
		{
			*pReturnError = PVRTStringFromFormattedStr("Too many lines of text in file (maximum is %d)\n", m_psContext->nMaxLines);
			return PVR_FAIL;
		}
	}

	return Parse(pReturnError) ? PVR_SUCCESS : PVR_FAIL;
}

/*!***************************************************************************
 @Function			ParseFromFile
 @Input				pszFileName		PFX file name
 @Output			pReturnError	error string
 @Return			EPVRTError		PVR_SUCCESS for success parsing file
									PVR_FAIL if file doesn't exist or is invalid
 @Description		Reads the PFX file and calls the parser.
*****************************************************************************/
EPVRTError CPVRTPFXParser::ParseFromFile(const char * const pszFileName, CPVRTString * const pReturnError)
{
	CPVRTResourceFile PfxFile(pszFileName);
	if (!PfxFile.IsOpen())
	{
		*pReturnError = CPVRTString("Unable to open file ") + pszFileName;
		return PVR_FAIL;
	}

	CPVRTString PfxFileString;
	const char* pPfxData = (const char*) PfxFile.DataPtr();

	// Is our shader resource file data null terminated?
	if(pPfxData[PfxFile.Size()-1] != '\0')
	{
		// If not create a temporary null-terminated string
		PfxFileString.assign(pPfxData, PfxFile.Size());
		pPfxData = PfxFileString.c_str();
	}

	return ParseFromMemory(pPfxData, pReturnError);
}

/*!***************************************************************************
 @Function			SetViewportSize
 @Input				uiWidth				New viewport width
 @Input				uiHeight			New viewport height
 @Return			bool				True on success
 @Description		Allows the current viewport size to be set. This value
					is used for calculating relative texture resolutions
*****************************************************************************/
bool CPVRTPFXParser::SetViewportSize(unsigned int uiWidth, unsigned int uiHeight)
{
	if(uiWidth > 0 && uiHeight > 0)
	{
		m_uiViewportWidth = uiWidth;
		m_uiViewportHeight = uiHeight;
		return true;
	}
	else
	{
		return false;
	}
}

/*!***************************************************************************
 @Function			GetEndTag
 @Input				pszTagName		tag name
 @Input				nStartLine		start line
 @Output			pnEndLine		line end tag found
 @Return			true if tag found
 @Description		Searches for end tag pszTagName from line nStartLine.
					Returns true and outputs the line number of the end tag if
					found, otherwise returning false.
*****************************************************************************/
bool CPVRTPFXParser::GetEndTag(const char *pszTagName, int nStartLine, int *pnEndLine)
{
	char pszEndTag[100];
	strcpy(pszEndTag, "[/");
	strcat(pszEndTag, pszTagName);
	strcat(pszEndTag, "]");

	for(unsigned int i = nStartLine; i < m_psContext->nNumLines; i++)
	{
		if(strcmp(pszEndTag, m_psContext->ppszEffectFile[i]) == 0)
		{
			*pnEndLine = i;
			return true;
		}
	}

	return false;
}

/*!***************************************************************************
 @Function			ReduceWhitespace
 @Output			line		output text
 @Input				line		input text
 @Description		Reduces all white space characters in the string to one
					blank space.
*****************************************************************************/
void CPVRTPFXParser::ReduceWhitespace(char *line)
{


	// convert tabs and newlines to ' '
	char *tmp = strpbrk (line, "\t\n");
	while(tmp != NULL)
	{
		*tmp = ' ';
		tmp = strpbrk (line, "\t\n");
	}

	// remove all whitespace at start
	while(line[0] == ' ')
	{
		// move chars along to omit whitespace
		int counter = 0;
		do{
			line[counter] = line[counter+1];
			counter++;
		}while(line[counter] != '\0');
	}

	// step through chars of line remove multiple whitespace
	for(int i=0; i < (int)strlen(line); i++)
	{
		// whitespace found
		if(line[i] == ' ')
		{
			// count number of whitespace chars
			int numWhiteChars = 0;
			while(line[i+1+numWhiteChars] == ' ')
			{
				numWhiteChars++;
			}

			// multiple whitespace chars found
			if(numWhiteChars>0)
			{
				// move chars along to omit whitespace
				int counter=1;
				while(line[i+counter] != '\0')
				{
					line[i+counter] = line[i+numWhiteChars+counter];
					counter++;
				}
			}
		}
	}

	// If there is no string then do not remove terminating white symbols
	if(!strlen(line))
	    return;

	// remove all whitespace from end
	while(line[strlen(line)-1] == ' ')
	{
		// move chars along to omit whitespace
		line[strlen(line)-1] = '\0';
	}
}

/*!***************************************************************************
 @Function			FindParameter
 @Output
 @Input
 @Description		Finds the parameter after the specified delimiting character and
					returns the parameter as a string. An empty string is returned
					if a parameter cannot be found

*****************************************************************************/
CPVRTString CPVRTPFXParser::FindParameter(char *aszSourceString, const CPVRTString &parameterTag, const CPVRTString &delimiter)
{
	CPVRTString returnString("");
	char* aszTagStart = strstr(aszSourceString, parameterTag.c_str());

	// Tag was found, so search for parameter
	if(aszTagStart)
	{
		char* aszDelimiterStart = strstr(aszTagStart, delimiter.c_str());
		char* aszSpaceStart = strstr(aszTagStart, " ");

		// Delimiter found
		if(aszDelimiterStart && (!aszSpaceStart ||(aszDelimiterStart < aszSpaceStart)))
		{
			// Create a string from the delimiter to the next space
			size_t strCount(strcspn(aszDelimiterStart, " "));
			aszDelimiterStart++;	// Skip =
			returnString.assign(aszDelimiterStart, strCount-1);
		}
	}

	return returnString;
}

/*!***************************************************************************
 @Function			ProcessKeywordParam
 @Output
 @Input
 @Description		Processes the node name or vector position parameters that
					can be assigned to render pass keywords (e.g. ENVMAPCUBE=(11.0, 22.0, 0.0))

	*****************************************************************************/
bool CPVRTPFXParser::ProcessKeywordParam(const CPVRTString &parameterString)
{
	if(parameterString.compare("") == 0)
	{
		return false;
	}

	if(parameterString[0] == '"')
	{
		CPVRTString stringParam("");

		/*
			Start from the character after the first ".
			Create a string from this point until the next occurance of
			a " character.
		*/

		for(int keywordElement = 1; keywordElement < (int)parameterString.length(); ++keywordElement)
		{
			if(parameterString[keywordElement] == '"')
			{
				// Reached the " delimiter, so break out of the loop
				break;
			}
			stringParam.append(parameterString[keywordElement]);
		}
		// Assign the node name and set the vector to (0,0,0)
		delete[]	m_psRenderPasses[m_nNumRenderPasses].pszNodeName;
		m_psRenderPasses[m_nNumRenderPasses].pszNodeName = new char[(stringParam.length()+1)];
		strcpy(m_psRenderPasses[m_nNumRenderPasses].pszNodeName, stringParam.c_str());

		m_psRenderPasses[m_nNumRenderPasses].vecPos = PVRTVec3(0,0,0);
		m_psRenderPasses[m_nNumRenderPasses].eCameraPosition = eFromNode;

	}else if(parameterString[0] == '(')
	{
		// Create a string for each vector element (x,y,z)
		CPVRTString vectorParamString[3];
		int vecId(0);
		for(int keywordElement = 1; keywordElement < (int)parameterString.length() && vecId < 3; ++keywordElement)
		{
			if(parameterString[keywordElement] == ',')
			{
				// Comma found, so increment vecId and continue to next char
				vecId++;
				continue;
			}
			else if(parameterString[keywordElement] == ')')
			{
				break;
			}

			// Append the current char to the current vector string
			vectorParamString[vecId].append(parameterString[keywordElement]);
		}

		// Store the retrieved values and set the node name to NULL
		m_psRenderPasses[m_nNumRenderPasses].vecPos = PVRTVec3(	(float)atof(vectorParamString[0].c_str()),
																(float)atof(vectorParamString[1].c_str()),
																(float)atof(vectorParamString[2].c_str()));

		delete[]	m_psRenderPasses[m_nNumRenderPasses].pszNodeName;
		m_psRenderPasses[m_nNumRenderPasses].pszNodeName = NULL;
		m_psRenderPasses[m_nNumRenderPasses].eCameraPosition = eFromExplicitPositon;
	}
	else
	{
		// Unrecognised data has been supplied
		m_psRenderPasses[m_nNumRenderPasses].eCameraPosition = eFromActiveCamera;
		return false;
	}
	return true;
}

/*!***************************************************************************
 @Function			ParseHeader
 @Input				nStartLine		start line number
 @Input				nEndLine		end line number
 @Output			pReturnError	error string
 @Return			bool			true if parse is successful
 @Description		Parses the HEADER section of the PFX file.
*****************************************************************************/
bool CPVRTPFXParser::ParseHeader(int nStartLine, int nEndLine, CPVRTString * const pReturnError)
{
	for(int i = nStartLine+1; i < nEndLine; i++)
	{
		// Skip blank lines
		if(!*m_psContext->ppszEffectFile[i])
			continue;

		char *str = strtok (m_psContext->ppszEffectFile[i]," ");
		if(str != NULL)
		{
			if(strcmp(str, "VERSION") == 0)
			{
				str += (strlen(str)+1);
				m_sHeader.pszVersion = (char *)malloc((strlen(str) + 1) * sizeof(char));
				strcpy(m_sHeader.pszVersion, str);
			}
			else if(strcmp(str, "DESCRIPTION") == 0)
			{
				str += (strlen(str)+1);
				m_sHeader.pszDescription = (char *)malloc((strlen(str) + 1) * sizeof(char));
				strcpy(m_sHeader.pszDescription, str);
			}
			else if(strcmp(str, "COPYRIGHT") == 0)
			{
				str += (strlen(str)+1);
				m_sHeader.pszCopyright = (char *)malloc((strlen(str) + 1) * sizeof(char));
				strcpy(m_sHeader.pszCopyright, str);
			}
			else
			{
				*pReturnError = PVRTStringFromFormattedStr("Unknown keyword '%s' in [HEADER] on line %d\n", str, m_psContext->pnFileLineNumber[i]);
				return false;
			}
		}
		else
		{
			*pReturnError = PVRTStringFromFormattedStr("Missing arguments in [HEADER] on line %d : %s\n", m_psContext->pnFileLineNumber[i],  m_psContext->ppszEffectFile[i]);
			return false;
		}
	}

	// initialise empty strings
	if(m_sHeader.pszVersion == NULL)
	{
		m_sHeader.pszVersion = (char *)malloc(sizeof(char));
		strcpy(m_sHeader.pszVersion, "");
	}
	if(m_sHeader.pszDescription == NULL)
	{
		m_sHeader.pszDescription = (char *)malloc(sizeof(char));
		strcpy(m_sHeader.pszDescription, "");
	}
	if(m_sHeader.pszCopyright == NULL)
	{
		m_sHeader.pszCopyright = (char *)malloc(sizeof(char));
		strcpy(m_sHeader.pszCopyright, "");
	}

	return true;
}

/*!***************************************************************************
 @Function			ParseTextures
 @Input				nStartLine		start line number
 @Input				nEndLine		end line number
 @Output			pReturnError	error string
 @Return			bool			true if parse is successful
 @Description		Parses the TEXTURE section of the PFX file.
*****************************************************************************/
bool CPVRTPFXParser::ParseTextures(int nStartLine, int nEndLine, CPVRTString * const pReturnError)
{
	char *pszName(NULL), *pszFile(NULL), *pszKeyword(NULL);
	char *pszRemaining(NULL), *pszTemp(NULL);
	bool bReturnVal(false);

	for(int i = nStartLine+1; i < nEndLine; i++)
	{
		// Skip blank lines
		if(!*m_psContext->ppszEffectFile[i])
			continue;

		char *str = strtok (m_psContext->ppszEffectFile[i]," ");
		if(str != NULL)
		{
			// Set defaults
			bool			bRenderToTexture(false);
			unsigned int	uiMin(eFilter_Default), uiMag(eFilter_Default), uiMip(eFilter_MipDefault);
			unsigned int	uiWrapS(eWrap_Default), uiWrapT(eWrap_Default), uiWrapR(eWrap_Default);
			unsigned int	uiFlags = 0;

			unsigned int uiWidth	= m_uiViewportWidth;	// NOTE: Change/remove default values?
			unsigned int uiHeight	= m_uiViewportHeight;

			// Reset variables
			FREE(pszName)		pszName = NULL;
			FREE(pszFile)		pszFile = NULL;
			FREE(pszKeyword)	pszKeyword = NULL;
			FREE(pszTemp)		pszTemp = NULL;
			pszRemaining		= NULL;

			// Compare against all valid keywords
			if((strcmp(str, "FILE") != 0) && (strcmp(str, "RENDER") != 0))
			{
				*pReturnError = PVRTStringFromFormattedStr("Unknown keyword '%s' in [TEXTURES] on line %d\n", str, m_psContext->pnFileLineNumber[i]);
				goto fail_release_return;
			}

			if((strcmp(str, "RENDER") == 0) && m_nNumRenderPasses == m_nMaxRenders)
			{
				*pReturnError = PVRTStringFromFormattedStr("Maximum number of render passes has been exceeded on line %d: %s\n", m_psContext->pnFileLineNumber[i], m_psContext->ppszEffectFile[i]);
				goto fail_release_return;
			}

			pszKeyword = (char *)malloc( ((int)strlen(str)+1) * sizeof(char));
			strcpy(pszKeyword, str);

			str = strtok (NULL, " ");
			if(str != NULL)
			{
				pszName = (char *)malloc( ((int)strlen(str)+1) * sizeof(char));
				strcpy(pszName, str);
				delete[]	m_psRenderPasses[m_nNumRenderPasses].pszSemanticName;	// Delete existing string
				m_psRenderPasses[m_nNumRenderPasses].pszSemanticName = new char[strlen(str)+1];
				strcpy(m_psRenderPasses[m_nNumRenderPasses].pszSemanticName, str);
			}
			else
			{
				*pReturnError = PVRTStringFromFormattedStr("Texture name missing in [TEXTURES] on line %d: %s\n", m_psContext->pnFileLineNumber[i], m_psContext->ppszEffectFile[i]);
				goto fail_release_return;
			}

			/*
				The pszRemaining string is used to look for remaining flags.
				This has the advantage of allowing flags to be order independent
				and makes it easier to ommit some flags, but still pick up others
				(the previous method made it diffifult to retrieve filtering info
				if flags before it were missing)
			*/
			pszRemaining  = strtok(NULL, "\n");

			if(pszRemaining == NULL)
			{
				*pReturnError = PVRTStringFromFormattedStr("Incomplete definition in [TEXTURES] on line %d: %s\n", m_psContext->pnFileLineNumber[i], m_psContext->ppszEffectFile[i]);
				goto fail_release_return;
			}
			else if(strcmp(pszKeyword, "FILE") == 0)
			{
				pszTemp = (char *)malloc( ((int)strlen(pszRemaining)+1) * sizeof(char));
				strcpy(pszTemp, pszRemaining);
				str = strtok (pszTemp, " ");

				if(str != NULL)
				{
					pszFile = (char *)malloc( ((int)strlen(str)+1) * sizeof(char));
					strcpy(pszFile, str);
				}
				else
				{
					*pReturnError = PVRTStringFromFormattedStr("Texture name missing in [TEXTURES] on line %d: %s\n", m_psContext->pnFileLineNumber[i], m_psContext->ppszEffectFile[i]);
					goto fail_release_return;
				}
			}
			else if(strcmp(pszKeyword, "RENDER") == 0)
			{
				/*
					TODO: When the parameters of the render texture have changed (such as format or resolution)
					the previous texture should be destroyed and replaced with the updated one.
					The texture ID may also need updating after this.
				*/
				// Use shader name instead of file name, as a file name doesn't exist
				pszFile = (char *)malloc( ((int)strlen(pszName)+1) * sizeof(char));
				strcpy(pszFile, pszName);
				bRenderToTexture = true;

				// Check for known keywords
				if(strstr(pszRemaining, "CAMERA") != NULL)
				{
					CPVRTString paramString(FindParameter(pszRemaining, "CAMERA", "="));

					if(paramString.compare("") != 0)
					{
						if(!ProcessKeywordParam(paramString))
						{
							*pReturnError = PVRTStringFromFormattedStr("Unknown or incorrectly formatted parameter has been given after CAMERA in [TEXTURES] on line %d: %s\n", m_psContext->pnFileLineNumber[i], m_psContext->ppszEffectFile[i]);
							goto fail_release_return;
						}
					}

					uiWidth = m_uiViewportWidth;
					uiHeight = m_uiViewportHeight;

					m_psRenderPasses[m_nNumRenderPasses].eRenderPassType = eCameraRender;
					m_psRenderPasses[m_nNumRenderPasses].eCameraPosition = eFromNode;
				}
//#endif /* POST_PROCESS_ENABLE */
				else
				{
					// Default
					*pReturnError = PVRTStringFromFormattedStr("An unknown render pass type was specified in [TEXTURES] on line %d: %s\n", m_psContext->pnFileLineNumber[i], m_psContext->ppszEffectFile[i]);
					goto fail_release_return;
				}
				m_psRenderPasses[m_nNumRenderPasses].i32TextureNumber = m_psTexture.GetSize();


				// ----------------Texture format----------------//
				// Find a FORMAT tag and isolate it's parameter string
				CPVRTString formatParamString = FindParameter(pszRemaining, "FORMAT", "=");

				// Check that a parameter string was found
				if(formatParamString.compare("") != 0)
				{
					// NOTE: Add more formats as required
					if(formatParamString.compare("RGBA_4444") == 0)
					{
						m_psRenderPasses[m_nNumRenderPasses].eFormat = OGL_RGBA_4444;
					}
					else if(formatParamString.compare("RGBA_5551") == 0)
					{
						m_psRenderPasses[m_nNumRenderPasses].eFormat = OGL_RGBA_5551;
					}
					else if(formatParamString.compare("RGBA_8888") == 0)
					{
						m_psRenderPasses[m_nNumRenderPasses].eFormat = OGL_RGBA_8888;
					}
					else if(formatParamString.compare("RGB_565") == 0)
					{
						m_psRenderPasses[m_nNumRenderPasses].eFormat = OGL_RGB_565;
					}
					else if(formatParamString.compare("RGB_555") == 0)
					{
						m_psRenderPasses[m_nNumRenderPasses].eFormat = OGL_RGB_555;
					}
					else if(formatParamString.compare("RGB_888") == 0)
					{
						m_psRenderPasses[m_nNumRenderPasses].eFormat = OGL_RGB_888;
					}
					else if(formatParamString.compare("I_8") == 0)
					{
						m_psRenderPasses[m_nNumRenderPasses].eFormat = OGL_I_8;
					}
					else if(formatParamString.compare("AI_88") == 0)
					{
						m_psRenderPasses[m_nNumRenderPasses].eFormat = OGL_AI_88;
					}
					else
					{
						*pReturnError = PVRTStringFromFormattedStr("Unrecognised texture format in [TEXTURES] on line %d\n", m_psContext->pnFileLineNumber[i]);
						goto fail_release_return;
					}
					uiFlags &= m_psRenderPasses[m_nNumRenderPasses].eFormat;
				}

				// ----------------Texture resolution----------------//
				// Search for the RES tag
				CPVRTString resParamString = FindParameter(pszRemaining, "RES", "=");

				// First, check for known keywords
				if(resParamString.compare("") != 0)
				{
					if(strstr(resParamString.c_str(), "SCREENRES") != NULL)
					{
						uiWidth = m_uiViewportWidth;
						uiHeight = m_uiViewportHeight;
					}
					else if(strstr(resParamString.c_str(), "SRESPOTLSQ") != NULL)
					{
						int i32StringLength((int)strlen("SRESPOTLSQ"));
						// String is larger than param name, so extra info is appended
						if((int)resParamString.length() > i32StringLength)
						{
							// Get a pointer to the start of the exponent
							char *aszExponent = &resParamString[i32StringLength];

							int i32Exponent = atoi(aszExponent);

							uiWidth = PVRTGetPOTLower(m_uiViewportWidth, i32Exponent);
							uiHeight = PVRTGetPOTLower(m_uiViewportHeight, i32Exponent);
						}
						else
						{
							// Set to next lowest POT res
							uiWidth = PVRTGetPOTLower(m_uiViewportWidth, 0);
							uiHeight = PVRTGetPOTLower(m_uiViewportHeight, 0);
						}

						/*
							Make a square POT texture.
							After calculating the POT of the width and height,
							use the lowest value
						*/
						if(uiWidth > uiHeight)
						{
							uiWidth = uiHeight;
						}
						else
						{
							uiHeight = uiWidth;
						}
					}
					else if(strstr(resParamString.c_str(), "SRESPOTHSQ") != NULL)
					{
						int i32StringLength((int)strlen("SRESPOTHSQ"));
						// String is larger than param name, so extra info is appended
						if((int)resParamString.length() > i32StringLength)
						{
							// Get a pointer to the start of the exponent
							char *aszExponent = &resParamString[i32StringLength];

							int i32Exponent = atoi(aszExponent);

							uiWidth = PVRTGetPOTHigher(m_uiViewportWidth, i32Exponent);
							uiHeight = PVRTGetPOTHigher(m_uiViewportHeight, i32Exponent);
						}
						else
						{
							// Set to next higher POT res
							uiWidth = PVRTGetPOTHigher(m_uiViewportWidth, 0);
							uiHeight = PVRTGetPOTHigher(m_uiViewportHeight, 0);
						}
						/*
							Make a square POT texture.
							After calculating the POT of the width and height,
							use the lowest value
						*/
						if(uiWidth > uiHeight)
						{
							uiWidth = uiHeight;
						}
						else
						{
							uiHeight = uiWidth;
						}
					}
					else if(strstr(resParamString.c_str(), "SRESPOTL") != NULL)
					{
						int i32StringLength((int)strlen("SRESPOTL"));
						// String is larger than param name, so extra info is appended
						if((int)resParamString.length() > i32StringLength)
						{
							// Get a pointer to the start of the exponent
							char *aszExponent = &resParamString[i32StringLength];

							int i32Exponent = atoi(aszExponent);

							uiWidth = PVRTGetPOTLower(m_uiViewportWidth, i32Exponent);
							uiHeight = PVRTGetPOTLower(m_uiViewportHeight, i32Exponent);
						}
						else
						{
							// Set to next lowest POT res
							uiWidth = PVRTGetPOTLower(m_uiViewportWidth, 0);
							uiHeight = PVRTGetPOTLower(m_uiViewportHeight, 0);
						}
					}
					else if(strstr(resParamString.c_str(), "SRESPOTH") != NULL)
					{
						int i32StringLength((int)strlen("SRESPOTH"));
						// String is larger than param name, so extra info is appended
						if((int)resParamString.length() > i32StringLength)
						{
							// Get a pointer to the start of the exponent
							char *aszExponent = &resParamString[i32StringLength];

							int i32Exponent = atoi(aszExponent);

							uiWidth = PVRTGetPOTHigher(m_uiViewportWidth, i32Exponent);
							uiHeight = PVRTGetPOTHigher(m_uiViewportHeight, i32Exponent);
						}
						else
						{
							// Set to next higher POT res
							uiWidth = PVRTGetPOTHigher(m_uiViewportWidth, 0);
							uiHeight = PVRTGetPOTHigher(m_uiViewportHeight, 0);
						}
					}
					else if(strstr(resParamString.c_str(), "SRESL") != NULL)
					{
						int i32StringLength((int)strlen("SRESL"));
						int i32Divider(2);

						// String is longer than param name, so divider has been provided
						if((int)resParamString.length() > i32StringLength)
						{
							// Get a pointer to the start of the divider
							char *aszDivider = &resParamString[i32StringLength];

							i32Divider = atoi(aszDivider);

							if(i32Divider <= 0)
							{
								*pReturnError = PVRTStringFromFormattedStr("Invalid divide value has been provided after SRESL in [TEXTURES] on line %d\n", m_psContext->pnFileLineNumber[i]);
								goto fail_release_return;
							}
						}

						uiWidth = m_uiViewportWidth / i32Divider;
						uiHeight = m_uiViewportHeight / i32Divider;
					}
					else if(strstr(resParamString.c_str(), "SRESH") != NULL)
					{
						int i32StringLength((int)strlen("SRESH"));
						int i32Multiplier(2);

						// String is longer than param name, so divider has been provided
						if((int)resParamString.length() > i32StringLength)
						{
							// Get a pointer to the start of the divider
							char *aszMultiplier = &resParamString[i32StringLength];

							i32Multiplier = atoi(aszMultiplier);

							if(i32Multiplier <= 0)
							{
								*pReturnError = PVRTStringFromFormattedStr("Invalid multiply value has been provided after SRESH in [TEXTURES] on line %d\n", m_psContext->pnFileLineNumber[i]);
								goto fail_release_return;
							}
						}

						uiWidth = m_uiViewportWidth * i32Multiplier;
						uiHeight = m_uiViewportHeight * i32Multiplier;
					}
					// Secondly, check for explicit resolutions
					else if(strstr(resParamString.c_str(), "x") != NULL)
					{
						size_t spanToX(strcspn(resParamString.c_str(), "x"));
						char *aszWidth = (char*) malloc((spanToX+1) * sizeof(char));
						strncpy(aszWidth, resParamString.c_str(), spanToX);
						int iWidth(atoi(aszWidth));
						FREE(aszWidth);
						int iHeight(atoi(&resParamString[++spanToX]));

						if(iWidth <= 0 || iHeight <= 0)
						{
							*pReturnError = PVRTStringFromFormattedStr("Invalid explicit resolution specified in [TEXTURES] on line %d\n", m_psContext->pnFileLineNumber[i]);
							goto fail_release_return;
						}
						else
						{
							uiWidth = (unsigned int)iWidth;
							uiHeight = (unsigned int)iHeight;
						}
					}
					else
					{
						*pReturnError = PVRTStringFromFormattedStr("Unrecognised resolution in [TEXTURES] on line %d\n", m_psContext->pnFileLineNumber[i]);
						goto fail_release_return;
					}
				}
			}

			if((strcmp(pszKeyword, "RENDER") == 0) || strcmp(pszKeyword, "FILE") == 0)
			{
				// ----------------Texture filtering----------------//
				// --- Filter flags
				{
					unsigned int* pFlags[3] =
					{
						&uiMin,
						&uiMag,
						&uiMip,
					};

					if(!ParseTextureFlags(pszRemaining, pFlags, 3, c_ppszFilters, eFilter_Size, pReturnError, i))
						goto fail_release_return;
				}

				// --- Wrap flags
				{
					unsigned int* pFlags[3] =
					{
						&uiWrapS,
						&uiWrapT,
						&uiWrapR,
					};

					if(!ParseTextureFlags(pszRemaining, pFlags, 3, c_ppszWraps, eWrap_Size, pReturnError, i))
						goto fail_release_return;
				}
	

				int i32Index = m_psTexture.Append();			// Get a new index.
				FREE(m_psTexture[i32Index].pszName);
				m_psTexture[i32Index].pszName = (char *)malloc( ((int)strlen(pszName)+1) * sizeof(char));
				strcpy(m_psTexture[i32Index].pszName, pszName);
				FREE(pszName);

				FREE(m_psTexture[i32Index].pszFile);
				m_psTexture[i32Index].pszFile = (char *)malloc( ((int)strlen(pszFile)+1) * sizeof(char));
				strcpy(m_psTexture[i32Index].pszFile, pszFile);
				FREE(pszFile);

				// Copy the values
				m_psTexture[i32Index].bRenderToTexture	= bRenderToTexture;
				m_psTexture[i32Index].nMin				= uiMin;
				m_psTexture[i32Index].nMag				= uiMag;
				m_psTexture[i32Index].nMIP				= uiMip;
				m_psTexture[i32Index].nWrapS			= uiWrapS;
				m_psTexture[i32Index].nWrapT			= uiWrapT;
				m_psTexture[i32Index].nWrapR			= uiWrapR;
				m_psTexture[i32Index].uiWidth			= uiWidth;
				m_psTexture[i32Index].uiHeight			= uiHeight;
				m_psTexture[i32Index].uiFlags			= uiFlags;

				if(strcmp(pszKeyword, "RENDER") == 0)
				{
					if(m_nNumRenderPasses < m_nMaxRenders)
					{
						m_nNumRenderPasses++;
					}
					else
					{
						*pReturnError = PVRTStringFromFormattedStr("Max number of render passes has been reached in [TEXTURES] on line %d: %s\n", m_psContext->pnFileLineNumber[i], m_psContext->ppszEffectFile[i]);
						goto fail_release_return;
					}
				}
			}
			else
			{
				*pReturnError = PVRTStringFromFormattedStr("Unknown keyword '%s' in [TEXTURES] on line %d\n", str, m_psContext->pnFileLineNumber[i]);;
				goto fail_release_return;
			}
		}
		else
		{
			*pReturnError = PVRTStringFromFormattedStr("Missing arguments in [TEXTURES] on line %d: %s\n", m_psContext->pnFileLineNumber[i],  m_psContext->ppszEffectFile[i]);
			goto fail_release_return;
		}
	}

	/*
		Should only reach here if there have been no issues
	*/
	bReturnVal = true;
	goto release_return;

fail_release_return:
	bReturnVal = false;
release_return:
	FREE(pszKeyword);
	FREE(pszName);
	FREE(pszFile);
	FREE(pszTemp);
	return bReturnVal;
}

/*!***************************************************************************
@Function		ParseTextureFlags
@Input			c_pszCursor
@Output			pFlagsOut
@Input			uiNumFlags
@Input			ppszFlagNames
@Input			uiNumFlagNames
@Input			pReturnError
@Input			iLineNum
@Return			bool	
@Description	Parses the texture flag sections.
*****************************************************************************/
bool CPVRTPFXParser::ParseTextureFlags(	const char* c_pszRemainingLine, unsigned int** ppFlagsOut, unsigned int uiNumFlags, const char** c_ppszFlagNames, unsigned int uiNumFlagNames, 
										CPVRTString * const pReturnError, int iLineNum)
{
	const unsigned int INVALID_TYPE = 0xAC1DBEEF;
	unsigned int uiIndex;
	const char* c_pszCursor;
	const char* c_pszResult;

	// --- Find the first flag
	uiIndex = 0;
	c_pszCursor = strstr(c_pszRemainingLine, c_ppszFlagNames[uiIndex++]);
	while(uiIndex < uiNumFlagNames)
	{
		c_pszResult = strstr(c_pszRemainingLine, c_ppszFlagNames[uiIndex++]);
		if(((c_pszResult < c_pszCursor) || !c_pszCursor) && c_pszResult)
			c_pszCursor = c_pszResult;
	}

	if(!c_pszCursor)
		return true;		// No error, but just return as no flags specified.

	// Quick error check - make sure that the first flag found is valid.
	if(c_pszCursor != c_pszRemainingLine)
	{
		if(*(c_pszCursor-1) == '-')		// Yeah this shouldn't be there. Must be invalid first tag.
		{
			char szBuffer[128];		// Find out the tag.
			memset(szBuffer, 0, sizeof(szBuffer));
			const char* pszStart = c_pszCursor-1;
			while(pszStart != c_pszRemainingLine && *pszStart != ' ')		pszStart--;
			pszStart++;	// Escape the space.
			unsigned int uiNumChars = (c_pszCursor-1) - pszStart;
			strncpy(szBuffer, pszStart, uiNumChars);

			*pReturnError = PVRTStringFromFormattedStr("Unknown keyword '%s' in [TEXTURES] on line %d: %s\n", szBuffer, m_psContext->pnFileLineNumber[iLineNum], m_psContext->ppszEffectFile[iLineNum]);
			return false;
		}
	}

	unsigned int uiFlagsFound = 0;
	unsigned int uiBufferIdx;
	char szBuffer[128];		// Buffer to hold the token

	while(*c_pszCursor != ' ' && *c_pszCursor != 0 && uiFlagsFound < uiNumFlags)
	{
		memset(szBuffer, 0, sizeof(szBuffer));		// Clear the buffer
		uiBufferIdx = 0;

		while(*c_pszCursor != '-' && *c_pszCursor != 0 && *c_pszCursor != ' ' && uiBufferIdx < 128)		// - = delim. token
			szBuffer[uiBufferIdx++] = *c_pszCursor++;

		// Check if the buffer content is a valid flag name.
		unsigned int Type = INVALID_TYPE;
		for(unsigned int uiIndex = 0; uiIndex < uiNumFlagNames; ++uiIndex)
		{
			if(strcmp(szBuffer, c_ppszFlagNames[uiIndex]) == 0)	
			{
				Type = uiIndex;			// Yup, it's valid. uiIndex here would translate to one of the enums that matches the string array of flag names passed in.
				break;
			}
		}

		// Tell the user it's invalid.
		if(Type == INVALID_TYPE)
		{
			*pReturnError = PVRTStringFromFormattedStr("Unknown keyword '%s' in [TEXTURES] on line %d: %s\n", szBuffer, m_psContext->pnFileLineNumber[iLineNum], m_psContext->ppszEffectFile[iLineNum]);
			return false;
		}

		// Set the flag to the enum type.
		*ppFlagsOut[uiFlagsFound++] = Type;

		if(*c_pszCursor == '-')	c_pszCursor++;
	}

	return true;
}

/*!***************************************************************************
 @Function			ParseShader
 @Input				nStartLine		start line number
 @Input				nEndLine		end line number
 @Output			pReturnError	error string
 @Output			shader			shader data object
 @Input				pszBlockName	name of block in PFX file
 @Return			bool			true if parse is successful
 @Description		Parses the VERTEXSHADER or FRAGMENTSHADER section of the
					PFX file.
*****************************************************************************/
bool CPVRTPFXParser::ParseShader(int nStartLine, int nEndLine, CPVRTString * const pReturnError, SPVRTPFXParserShader &shader, const char * const pszBlockName)
{
	bool glslcode=0, glslfile=0, bName=0;

	shader.pszName			= NULL;
	shader.bUseFileName		= false;
	shader.pszGLSLfile		= NULL;
	shader.pszGLSLcode		= NULL;
	shader.pszGLSLBinaryFile= NULL;
	shader.pbGLSLBinary		= NULL;
	shader.nFirstLineNumber	= 0;

	for(int i = nStartLine+1; i < nEndLine; i++)
	{
		// Skip blank lines
		if(!*m_psContext->ppszEffectFile[i])
			continue;

		char *str = strtok (m_psContext->ppszEffectFile[i]," ");
		if(str != NULL)
		{
			// Check for [GLSL_CODE] tags first and remove those lines from loop.
			if(strcmp(str, "[GLSL_CODE]") == 0)
			{
				if(glslcode)
				{
					*pReturnError = PVRTStringFromFormattedStr("[GLSL_CODE] redefined in [%s] on line %d\n", pszBlockName, m_psContext->pnFileLineNumber[i]);
					return false;
				}
				if(glslfile && shader.pbGLSLBinary==NULL )
				{
					*pReturnError = PVRTStringFromFormattedStr("[GLSL_CODE] not allowed with FILE in [%s] on line %d\n", pszBlockName, m_psContext->pnFileLineNumber[i]);
					return false;
				}

				shader.nFirstLineNumber = m_psContext->pnFileLineNumber[i];

				// Skip the block-start
				i++;

				if(!ConcatenateLinesUntil(
					shader.pszGLSLcode,
					i,
					m_psContext->ppszEffectFile,
					m_psContext->nNumLines,
					"[/GLSL_CODE]"))
				{
					return false;
				}

				shader.bUseFileName = false;
				glslcode = 1;
			}
			else if(strcmp(str, "NAME") == 0)
			{
				if(bName)
				{
					*pReturnError = PVRTStringFromFormattedStr("NAME redefined in [%s] on line %d\n", pszBlockName, m_psContext->pnFileLineNumber[i]);
					return false;
				}

				str = ReadEOLToken(NULL);

				if(str == NULL)
				{
					*pReturnError = PVRTStringFromFormattedStr("NAME missing value in [%s] on line %d\n", pszBlockName, m_psContext->pnFileLineNumber[i]);
					return false;
				}

				shader.pszName = (char*)malloc((strlen(str)+1) * sizeof(char));
				strcpy(shader.pszName, str);
				bName = true;
			}
			else if(strcmp(str, "FILE") == 0)
			{
				if(glslfile)
				{
					*pReturnError = PVRTStringFromFormattedStr("FILE redefined in [%s] on line %d\n", pszBlockName, m_psContext->pnFileLineNumber[i]);
					return false;
				}
				if(glslcode)
				{
					*pReturnError = PVRTStringFromFormattedStr("FILE not allowed with [GLSL_CODE] in [%s] on line %d\n", pszBlockName, m_psContext->pnFileLineNumber[i]);
					return false;
				}

				str = ReadEOLToken(NULL);

				if(str == NULL)
				{
					*pReturnError = PVRTStringFromFormattedStr("FILE missing value in [%s] on line %d\n", pszBlockName, m_psContext->pnFileLineNumber[i]);
					return false;
				}

				shader.pszGLSLfile = (char*)malloc((strlen(str)+1) * sizeof(char));
				strcpy(shader.pszGLSLfile, str);

				CPVRTResourceFile GLSLFile(str);

				if(!GLSLFile.IsOpen())
				{
					*pReturnError = PVRTStringFromFormattedStr("Error loading file '%s' in [%s] on line %d\n", str, pszBlockName, m_psContext->pnFileLineNumber[i]);
					return false;
				}
				shader.pszGLSLcode = new char[GLSLFile.Size() + 1];
				memcpy(shader.pszGLSLcode, (const char*) GLSLFile.DataPtr(), GLSLFile.Size());
				shader.pszGLSLcode[GLSLFile.Size()] = '\0';

				shader.bUseFileName = true;
				glslfile = 1;
			}
			else if(strcmp(str, "BINARYFILE") == 0)
			{
				str = ReadEOLToken(NULL);

				if(str == NULL)
				{
					*pReturnError = PVRTStringFromFormattedStr("BINARYFILE missing value in [%s] on line %d\n", pszBlockName, m_psContext->pnFileLineNumber[i]);
					return false;
				}

				shader.pszGLSLBinaryFile = (char*)malloc((strlen(str)+1) * sizeof(char));
				strcpy(shader.pszGLSLBinaryFile, str);

				CPVRTResourceFile GLSLFile(str);

				if(!GLSLFile.IsOpen())
				{
					*pReturnError = PVRTStringFromFormattedStr("Error loading file '%s' in [%s] on line %d\n", str, pszBlockName, m_psContext->pnFileLineNumber[i]);
					return false;
				}
				shader.pbGLSLBinary = new char[GLSLFile.Size()];
				shader.nGLSLBinarySize = (unsigned int)GLSLFile.Size();
				memcpy(shader.pbGLSLBinary, GLSLFile.DataPtr(), GLSLFile.Size());

				shader.bUseFileName = true;
				glslfile = 1;
			}
			else
			{
				*pReturnError = PVRTStringFromFormattedStr("Unknown keyword '%s' in [%s] on line %d\n", str, pszBlockName, m_psContext->pnFileLineNumber[i]);
				return false;
			}

			str = strtok (NULL, " ");
			if(str != NULL)
			{
				*pReturnError = PVRTStringFromFormattedStr("Unexpected data in [%s] on line %d: '%s'\n", pszBlockName, m_psContext->pnFileLineNumber[i], str);
				return false;
			}
		}
		else
		{
			*pReturnError = PVRTStringFromFormattedStr("Missing arguments in [%s] on line %d: %s\n", pszBlockName, m_psContext->pnFileLineNumber[i], m_psContext->ppszEffectFile[i]);
			return false;
		}
	}

	if(!bName)
	{
		*pReturnError = PVRTStringFromFormattedStr("NAME not found in [%s] on line %d.\n", pszBlockName, m_psContext->pnFileLineNumber[nStartLine]);
		return false;
	}

	if(!glslfile && !glslcode)
	{
		*pReturnError = PVRTStringFromFormattedStr("No Shader File or Shader Code specified in [%s] on line %d\n", pszBlockName, m_psContext->pnFileLineNumber[nStartLine]);
		return false;
	}

	return true;
}

/*!***************************************************************************
 @Function			ParseSemantic
 @Output			semantic		semantic data object
 @Input				nStartLine		start line number
 @Output			pReturnError	error string
 @Return			bool			true if parse is successful
 @Description		Parses a semantic.
*****************************************************************************/
bool CPVRTPFXParser::ParseSemantic(SPVRTPFXParserSemantic &semantic, const int nStartLine, CPVRTString * const pReturnError)
{
	char *str;

	semantic.pszName = 0;
	semantic.pszValue = 0;
	semantic.sDefaultValue.eType = eDataTypeNone;
	semantic.nIdx = 0;

	str = strtok (NULL, " ");
	if(str == NULL)
	{
		*pReturnError = PVRTStringFromFormattedStr("UNIFORM missing name in [EFFECT] on line %d\n", m_psContext->pnFileLineNumber[nStartLine]);
		return false;
	}
	semantic.pszName = (char*)malloc((strlen(str)+1) * sizeof(char));
	strcpy(semantic.pszName, str);

	str = strtok (NULL, " ");
	if(str == NULL)
	{
		*pReturnError = PVRTStringFromFormattedStr("UNIFORM missing value in [EFFECT] on line %d\n", m_psContext->pnFileLineNumber[nStartLine]);

		FREE(semantic.pszName);
		return false;
	}

	/*
		If the final digits of the semantic are a number they are
		stripped off and used as the index, with the remainder
		used as the semantic.
	*/
	{
		size_t idx, len;
		len = strlen(str);

		idx = len;
		while(idx)
		{
			--idx;
			if(strcspn(&str[idx], "0123456789") != 0)
			{
				break;
			}
		}
		if(idx == 0)
		{
			*pReturnError = PVRTStringFromFormattedStr("Semantic contains only numbers in [EFFECT] on line %d\n", m_psContext->pnFileLineNumber[nStartLine]);

			FREE(semantic.pszName);
			return false;
		}

		++idx;
		// Store the semantic index
		if(len == idx)
		{
			semantic.nIdx = 0;
		}
		else
		{
			semantic.nIdx = atoi(&str[idx]);
		}

		// Chop off the index from the string containing the semantic
		str[idx] = 0;
	}

	// Store a copy of the semantic name
	semantic.pszValue = (char*)malloc((strlen(str)+1) * sizeof(char));
	strcpy(semantic.pszValue, str);

	/*
		Optional default semantic value
	*/
	char pszString[2048];
	strcpy(pszString,"");
	str = strtok (NULL, " ");
	if(str != NULL)
	{
		// Get all ramainning arguments
		while(str != NULL)
		{
			strcat(pszString, str);
			strcat(pszString, " ");
			str = strtok (NULL, " ");
		}

		// default value
		int i;
		for(i = 0; i < eNumDefaultDataTypes; i++)
		{
			if(strncmp(pszString, c_psSemanticDefaultDataTypeInfo[i].pszName, strlen(c_psSemanticDefaultDataTypeInfo[i].pszName)) == 0)
			{
				if(!GetSemanticDataFromString(	&semantic.sDefaultValue,
												&pszString[strlen(c_psSemanticDefaultDataTypeInfo[i].pszName)],
												c_psSemanticDefaultDataTypeInfo[i].eType,
												pReturnError
												))
				{
					*pReturnError = PVRTStringFromFormattedStr(" on line %d.\n", m_psContext->pnFileLineNumber[nStartLine]);

					FREE(semantic.pszValue);
					FREE(semantic.pszName);
					return false;
				}

				semantic.sDefaultValue.eType = c_psSemanticDefaultDataTypeInfo[i].eType;
				break;
			}
		}

		// invalid data type
		if(i == eNumDefaultDataTypes)
		{
			*pReturnError = PVRTStringFromFormattedStr("'%s' unknown on line %d.\n", pszString, m_psContext->pnFileLineNumber[nStartLine]);

			FREE(semantic.pszValue);
			FREE(semantic.pszName);
			return false;
		}

	}

	return true;
}

/*!***************************************************************************
 @Function			ParseEffect
 @Output			effect			effect data object
 @Input				nStartLine		start line number
 @Input				nEndLine		end line number
 @Output			pReturnError	error string
 @Return			bool			true if parse is successful
 @Description		Parses the EFFECT section of the PFX file.
*****************************************************************************/
bool CPVRTPFXParser::ParseEffect(SPVRTPFXParserEffect &effect, const int nStartLine, const int nEndLine, CPVRTString * const pReturnError)
{
	bool bName = false;
	bool bVertShader = false;
	bool bFragShader = false;

	effect.pszName					= NULL;
	effect.pszAnnotation			= NULL;
	effect.pszVertexShaderName		= NULL;
	effect.pszFragmentShaderName	= NULL;

	effect.nMaxTextures				= 100;
	effect.nNumTextures				= 0;
	effect.psTextures				= new SPVRTPFXParserEffectTexture[effect.nMaxTextures];

	effect.nMaxUniforms				= 100;
	effect.nNumUniforms				= 0;
	effect.psUniform				= new SPVRTPFXParserSemantic[effect.nMaxUniforms];

	effect.nMaxAttributes			= 100;
	effect.nNumAttributes			= 0;
	effect.psAttribute				= new SPVRTPFXParserSemantic[effect.nMaxAttributes];

	for(int i = nStartLine+1; i < nEndLine; i++)
	{
		// Skip blank lines
		if(!*m_psContext->ppszEffectFile[i])
			continue;

		char *str = strtok (m_psContext->ppszEffectFile[i]," ");
		if(str != NULL)
		{
			if(strcmp(str, "[ANNOTATION]") == 0)
			{
				if(effect.pszAnnotation)
				{
					*pReturnError = PVRTStringFromFormattedStr("ANNOTATION redefined in [EFFECT] on line %d: \n", m_psContext->pnFileLineNumber[i]);
					return false;
				}

				i++;		// Skip the block-start
				if(!ConcatenateLinesUntil(
					effect.pszAnnotation,
					i,
					m_psContext->ppszEffectFile,
					m_psContext->nNumLines,
					"[/ANNOTATION]"))
				{
					return false;
				}
			}
			else if(strcmp(str, "VERTEXSHADER") == 0)
			{
				if(bVertShader)
				{
					*pReturnError = PVRTStringFromFormattedStr("VERTEXSHADER redefined in [EFFECT] on line %d: \n", m_psContext->pnFileLineNumber[i]);
					return false;
				}

				str = ReadEOLToken(NULL);

				if(str == NULL)
				{
					*pReturnError = PVRTStringFromFormattedStr("VERTEXSHADER missing value in [EFFECT] on line %d\n", m_psContext->pnFileLineNumber[i]);
					return false;
				}
				effect.pszVertexShaderName = (char*)malloc((strlen(str)+1) * sizeof(char));
				strcpy(effect.pszVertexShaderName, str);

				bVertShader = true;
			}
			else if(strcmp(str, "FRAGMENTSHADER") == 0)
			{
				if(bFragShader)
				{
					*pReturnError = PVRTStringFromFormattedStr("FRAGMENTSHADER redefined in [EFFECT] on line %d: \n", m_psContext->pnFileLineNumber[i]);
					return false;
				}

				str = ReadEOLToken(NULL);

				if(str == NULL)
				{
					*pReturnError = PVRTStringFromFormattedStr("FRAGMENTSHADER missing value in [EFFECT] on line %d\n", m_psContext->pnFileLineNumber[i]);
					return false;
				}
				effect.pszFragmentShaderName = (char*)malloc((strlen(str)+1) * sizeof(char));
				strcpy(effect.pszFragmentShaderName, str);

				bFragShader = true;
			}
			else if(strcmp(str, "TEXTURE") == 0)
			{
				if(effect.nNumTextures < effect.nMaxTextures)
				{
					// texture number
					str = strtok(NULL, " ");
					if(str != NULL)
						effect.psTextures[effect.nNumTextures].nNumber = atoi(str);
					else
					{
						*pReturnError = PVRTStringFromFormattedStr("TEXTURE missing value in [EFFECT] on line %d\n", m_psContext->pnFileLineNumber[i]);
						return false;
					}

					// texture name
					str = strtok(NULL, " ");
					if(str != NULL)
					{
						effect.psTextures[effect.nNumTextures].pszName = (char*)malloc(strlen(str) + 1);
						strcpy(effect.psTextures[effect.nNumTextures].pszName, str);
					}
					else
					{
						*pReturnError = PVRTStringFromFormattedStr("TEXTURE missing value in [EFFECT] on line %d\n", m_psContext->pnFileLineNumber[i]);
						return false;
					}

					++effect.nNumTextures;
				}
				else
				{
					*pReturnError = PVRTStringFromFormattedStr("Too many textures in [EFFECT] on line %d\n", m_psContext->pnFileLineNumber[i]);
					return false;
				}
			}
			else if(strcmp(str, "UNIFORM") == 0)
			{
				if(effect.nNumUniforms < effect.nMaxUniforms)
				{
					if(!ParseSemantic(effect.psUniform[effect.nNumUniforms], i, pReturnError))
						return false;
					effect.nNumUniforms++;
				}
				else
				{
					*pReturnError = PVRTStringFromFormattedStr("Too many uniforms in [EFFECT] on line %d\n", m_psContext->pnFileLineNumber[i]);
					return false;
				}
			}
			else if(strcmp(str, "ATTRIBUTE") == 0)
			{
				if(effect.nNumAttributes < effect.nMaxAttributes)
				{
					if(!ParseSemantic(effect.psAttribute[effect.nNumAttributes], i, pReturnError))
						return false;
					effect.nNumAttributes++;
				}
				else
				{
					*pReturnError = PVRTStringFromFormattedStr("Too many attributes in [EFFECT] on line %d\n", m_psContext->pnFileLineNumber[i]);
					return false;
				}
			}
			else if(strcmp(str, "NAME") == 0)
			{
				if(bName)
				{
					*pReturnError = PVRTStringFromFormattedStr("NAME redefined in [EFFECT] on line %d\n", m_psContext->pnFileLineNumber[nStartLine]);
					return false;
				}

				str = strtok (NULL, " ");
				if(str == NULL)
				{
					*pReturnError = PVRTStringFromFormattedStr("NAME missing value in [EFFECT] on line %d\n", m_psContext->pnFileLineNumber[nStartLine]);
					return false;
				}

				effect.pszName = (char *)malloc((strlen(str)+1) * sizeof(char));
				strcpy(effect.pszName, str);
				bName = true;
			}
			else
			{
				*pReturnError = PVRTStringFromFormattedStr("Unknown keyword '%s' in [EFFECT] on line %d\n", str, m_psContext->pnFileLineNumber[i]);
				return false;
			}
		}
		else
		{
			*pReturnError = PVRTStringFromFormattedStr( "Missing arguments in [EFFECT] on line %d: %s\n", m_psContext->pnFileLineNumber[i], m_psContext->ppszEffectFile[i]);
			return false;
		}

	}

	if(!bName)
	{
		*pReturnError = PVRTStringFromFormattedStr("No 'NAME' found in [EFFECT] on line %d\n", m_psContext->pnFileLineNumber[nStartLine]);
		return false;
	}
	if(!bVertShader)
	{
		*pReturnError = PVRTStringFromFormattedStr("No 'VERTEXSHADER' defined in [EFFECT] starting on line %d: \n", m_psContext->pnFileLineNumber[nStartLine-1]);
		return false;
	}
	if(!bFragShader)
	{
		*pReturnError = PVRTStringFromFormattedStr("No 'FRAGMENTSHADER' defined in [EFFECT] starting on line %d: \n", m_psContext->pnFileLineNumber[nStartLine-1]);
		return false;
	}

	return true;
}

/*!***************************************************************************
@Function			DebugDump
@Return				string A string containing debug information for the user
					to handle
@Description		Debug output.
*****************************************************************************/
CPVRTString CPVRTPFXParser::DebugDump() const
{
	unsigned int i;
	CPVRTString debug;

	debug += CPVRTString("[HEADER]\n");
	debug += PVRTStringFromFormattedStr("VERSION		%s\n", m_sHeader.pszVersion);
	debug += PVRTStringFromFormattedStr("DESCRIPTION		%s\n", m_sHeader.pszDescription);
	debug += PVRTStringFromFormattedStr("COPYRIGHT		%s\n", m_sHeader.pszCopyright);
	debug += CPVRTString("[/HEADER]\n\n");

	debug += CPVRTString("[TEXTURES]\n");
	for(i = 0; i < (unsigned int)m_psTexture.GetSize(); ++i)
	{
		debug += PVRTStringFromFormattedStr("FILE		%s		%s\n", m_psTexture[i].pszName, m_psTexture[i].pszFile);
	}
	debug += CPVRTString("[/TEXTURES]\n\n");

	debug += CPVRTString("[VERTEXSHADER]\n");
	debug += PVRTStringFromFormattedStr("NAME		%s\n",	m_psVertexShader[0].pszName);
	debug += PVRTStringFromFormattedStr("GLSLFILE		%s\n",	m_psVertexShader[0].pszGLSLfile);
	debug += CPVRTString("[GLSL_CODE]\n");
	debug += PVRTStringFromFormattedStr("%s", m_psVertexShader[0].pszGLSLcode);
	debug += CPVRTString("[/GLSL_CODE]\n");
	debug += CPVRTString("[/VERTEXSHADER]\n\n");

	debug += CPVRTString("[FRAGMENTSHADER]\n");
	debug += PVRTStringFromFormattedStr("NAME		%s\n",	m_psFragmentShader[0].pszName);
	debug += PVRTStringFromFormattedStr("GLSLFILE		%s\n",	m_psFragmentShader[0].pszGLSLfile);
	debug += CPVRTString("[GLSL_CODE]\n");
	debug += PVRTStringFromFormattedStr("%s", m_psFragmentShader[0].pszGLSLcode);
	debug += CPVRTString("[/GLSL_CODE]\n");
	debug += CPVRTString("[/FRAGMENTSHADER]\n\n");

	for(unsigned int nEffect = 0; nEffect < (unsigned int)m_psEffect.GetSize(); ++nEffect)
	{
		debug += CPVRTString("[EFFECT]\n");

		debug += PVRTStringFromFormattedStr("NAME		%s\n",	m_psEffect[nEffect].pszName);
		debug += PVRTStringFromFormattedStr("[ANNOTATION]\n%s[/ANNOTATION]\n",	m_psEffect[nEffect].pszAnnotation);
		debug += PVRTStringFromFormattedStr("FRAGMENTSHADER		%s\n",	m_psEffect[nEffect].pszFragmentShaderName);
		debug += PVRTStringFromFormattedStr("VERTEXSHADER		%s\n",	m_psEffect[nEffect].pszVertexShaderName);

		for(i=0; i<m_psEffect[nEffect].nNumTextures; i++)
		{
			debug += PVRTStringFromFormattedStr("TEXTURE		%d		%s\n", m_psEffect[nEffect].psTextures[i].nNumber, m_psEffect[nEffect].psTextures[i].pszName);
		}

		for(i=0; i<m_psEffect[nEffect].nNumUniforms; i++)
		{
			debug += PVRTStringFromFormattedStr("UNIFORM		%s		%s%d\n", m_psEffect[nEffect].psUniform[i].pszName, m_psEffect[nEffect].psUniform[i].pszValue, m_psEffect[nEffect].psUniform[i].nIdx);
		}

		for(i=0; i<m_psEffect[nEffect].nNumAttributes; i++)
		{
			debug += PVRTStringFromFormattedStr("ATTRIBUTE		%s		%s%d\n", m_psEffect[nEffect].psAttribute[i].pszName, m_psEffect[nEffect].psAttribute[i].pszValue, m_psEffect[nEffect].psAttribute[i].nIdx);
		}

		debug += CPVRTString("[/EFFECT]\n\n");
	}

	return debug;
}

/*!***************************************************************************
@Function		FindTextureIndex
@Input			TextureName
@Return			unsigned int	Index in to the effect.Texture array.
@Description	Returns the index in to the texture array within the effect 
				block where the given texture resides.
*****************************************************************************/
unsigned int CPVRTPFXParser::FindTextureIndex( const CPVRTString& TextureName, unsigned int uiEffect )
{
	// TODO: Remove string compare. Change to hash check.
	for(unsigned int uiIndex = 0; uiIndex < m_psEffect[uiEffect].nNumTextures; ++uiIndex)
	{
		SPVRTPFXParserEffectTexture* pTex = &m_psEffect[uiEffect].psTextures[uiIndex];
		if(strcmp(pTex->pszName, TextureName.c_str()) == 0)
		{
			return uiIndex;
		}
	}

	return 0xFFFFFFFF;
}

void PVRTPFXCreateStringCopy(char** ppDst, const char* pSrc)
{
	if(pSrc)
	{
		FREE(*ppDst);
		*ppDst = (char*)malloc((strlen(pSrc)+1) * sizeof(char));
		strcpy(*ppDst, pSrc);
	}
}

/*****************************************************************************
 End of file (PVRTPFXParser.cpp)
*****************************************************************************/

