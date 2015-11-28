<?xml version="1.0"?>
<xsl:stylesheet	xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0"
				xmlns:assignments="http://www.iana.org/assignments">
	<xsl:output method="text" encoding="us-ascii"/>
	<xsl:variable name="lwr" select="'abcdefghijklmnopqrstuvwxyz'"/>
	<xsl:variable name="upr" select="'ABCDEFGHIJKLMNOPQRSTUVWXYZ'"/>
	<xsl:template match="/assignments:registry">
		<xsl:text>/* Generated by character-sets.c.xsl */&#xa;&#xa;</xsl:text>
		<xsl:text>#define $(ID, CP, NAME) const struct \&#xa;</xsl:text>
		<xsl:text>	{ \&#xa;</xsl:text>
		<xsl:text>		unsigned short id; \&#xa;</xsl:text>
		<xsl:text>		unsigned short cp; \&#xa;</xsl:text>
		<xsl:text>		char name[sizeof NAME]; \&#xa;</xsl:text>
		<xsl:text>	} $##ID = { ID, CP, NAME };&#xa;&#xa;</xsl:text>
		<xsl:apply-templates select="assignments:registry"/>
	</xsl:template>
	<xsl:template match="assignments:registry">
		<xsl:apply-templates select="assignments:record"/>
	</xsl:template>
	<xsl:template match="assignments:record">
		<xsl:text>$(</xsl:text>
		<xsl:value-of select="assignments:value"/>
		<xsl:choose>
			<xsl:when test="assignments:value='3'">,	20127,	</xsl:when>
			<xsl:when test="assignments:value='4'">,	28591,	</xsl:when>
			<xsl:when test="assignments:value='5'">,	28592,	</xsl:when>
			<xsl:when test="assignments:value='6'">,	28593,	</xsl:when>
			<xsl:when test="assignments:value='7'">,	28594,	</xsl:when>
			<xsl:when test="assignments:value='8'">,	28595,	</xsl:when>
			<xsl:when test="assignments:value='9'">,	28596,	</xsl:when>
			<xsl:when test="assignments:value='10'">,	28597,	</xsl:when>
			<xsl:when test="assignments:value='11'">,	28598,	</xsl:when>
			<xsl:when test="assignments:value='12'">,	28599,	</xsl:when>
			<xsl:when test="assignments:value='17'">,	932,	</xsl:when>
			<xsl:when test="assignments:value='18'">,	51932,	</xsl:when>
			<xsl:when test="assignments:value='24'">,	20106,	</xsl:when>
			<xsl:when test="assignments:value='25'">,	20108,	</xsl:when>
			<xsl:when test="assignments:value='30'">,	20105,	</xsl:when>
			<xsl:when test="assignments:value='35'">,	20107,	</xsl:when>
			<xsl:when test="assignments:value='36'">,	949,	</xsl:when>
			<xsl:when test="assignments:value='37'">,	50225,	</xsl:when>
			<xsl:when test="assignments:value='38'">,	51949,	</xsl:when>
			<xsl:when test="assignments:value='39'">,	50220,	</xsl:when>
			<xsl:when test="assignments:value='57'">,	936,	</xsl:when>
			<xsl:when test="assignments:value='85'">,	38598,	</xsl:when>
			<xsl:when test="assignments:value='103'">,	65000,	</xsl:when>
			<xsl:when test="assignments:value='106'">,	65001,	</xsl:when>
			<xsl:when test="assignments:value='109'">,	28603,	</xsl:when>
			<xsl:when test="assignments:value='111'">,	28605,	</xsl:when>
			<xsl:when test="assignments:value='113'">,	936,	</xsl:when>
			<xsl:when test="assignments:value='114'">,	54936,	</xsl:when>
			<xsl:when test="assignments:value='1000'">,	1200,	</xsl:when>
			<xsl:when test="assignments:value='1012'">,	65000,	</xsl:when>
			<xsl:when test="assignments:value='1013'">,	1201,	</xsl:when>
			<xsl:when test="assignments:value='1014'">,	1200,	</xsl:when>
			<xsl:when test="assignments:value='1015'">,	1200,	</xsl:when>
			<xsl:when test="assignments:value='2009'">,	850,	</xsl:when>
			<xsl:when test="assignments:value='2013'">,	862,	</xsl:when>
			<xsl:when test="assignments:value='2016'">,	20838,	</xsl:when>
			<xsl:when test="assignments:value='2024'">,	932,	</xsl:when>
			<xsl:when test="assignments:value='2025'">,	936,	</xsl:when>
			<xsl:when test="assignments:value='2026'">,	950,	</xsl:when>
			<xsl:when test="assignments:value='2027'">,	10000,	</xsl:when>
			<xsl:when test="assignments:value='2028'">,	37,		</xsl:when>
			<xsl:when test="assignments:value='2030'">,	20273,	</xsl:when>
			<xsl:when test="assignments:value='2033'">,	20277,	</xsl:when>
			<xsl:when test="assignments:value='2034'">,	20278,	</xsl:when>
			<xsl:when test="assignments:value='2035'">,	20280,	</xsl:when>
			<xsl:when test="assignments:value='2037'">,	20284,	</xsl:when>
			<xsl:when test="assignments:value='2038'">,	20285,	</xsl:when>
			<xsl:when test="assignments:value='2039'">,	20290,	</xsl:when>
			<xsl:when test="assignments:value='2040'">,	20297,	</xsl:when>
			<xsl:when test="assignments:value='2041'">,	20420,	</xsl:when>
			<xsl:when test="assignments:value='2042'">,	20423,	</xsl:when>
			<xsl:when test="assignments:value='2043'">,	20424,	</xsl:when>
			<xsl:when test="assignments:value='2011'">,	437,	</xsl:when>
			<xsl:when test="assignments:value='2044'">,	500,	</xsl:when>
			<xsl:when test="assignments:value='2010'">,	852,	</xsl:when>
			<xsl:when test="assignments:value='2046'">,	855,	</xsl:when>
			<xsl:when test="assignments:value='2047'">,	857,	</xsl:when>
			<xsl:when test="assignments:value='2048'">,	860,	</xsl:when>
			<xsl:when test="assignments:value='2049'">,	861,	</xsl:when>
			<xsl:when test="assignments:value='2050'">,	863,	</xsl:when>
			<xsl:when test="assignments:value='2051'">,	864,	</xsl:when>
			<xsl:when test="assignments:value='2052'">,	865,	</xsl:when>
			<xsl:when test="assignments:value='2054'">,	869,	</xsl:when>
			<xsl:when test="assignments:value='2055'">,	870,	</xsl:when>
			<xsl:when test="assignments:value='2056'">,	20871,	</xsl:when>
			<xsl:when test="assignments:value='2057'">,	20880,	</xsl:when>
			<xsl:when test="assignments:value='2061'">,	20905,	</xsl:when>
			<xsl:when test="assignments:value='2063'">,	1026,	</xsl:when>
			<xsl:when test="assignments:value='2084'">,	20866,	</xsl:when>
			<xsl:when test="assignments:value='2085'">,	52936,	</xsl:when>
			<xsl:when test="assignments:value='2086'">,	866,	</xsl:when>
			<xsl:when test="assignments:value='2087'">,	775,	</xsl:when>
			<xsl:when test="assignments:value='2088'">,	21866,	</xsl:when>
			<xsl:when test="assignments:value='2089'">,	858,	</xsl:when>
			<xsl:when test="assignments:value='2090'">,	20924,	</xsl:when>
			<xsl:when test="assignments:value='2091'">,	1140,	</xsl:when>
			<xsl:when test="assignments:value='2092'">,	1141,	</xsl:when>
			<xsl:when test="assignments:value='2093'">,	1142,	</xsl:when>
			<xsl:when test="assignments:value='2094'">,	1143,	</xsl:when>
			<xsl:when test="assignments:value='2095'">,	1144,	</xsl:when>
			<xsl:when test="assignments:value='2096'">,	1145,	</xsl:when>
			<xsl:when test="assignments:value='2097'">,	1146,	</xsl:when>
			<xsl:when test="assignments:value='2098'">,	1147,	</xsl:when>
			<xsl:when test="assignments:value='2099'">,	1148,	</xsl:when>
			<xsl:when test="assignments:value='2100'">,	1149,	</xsl:when>
			<xsl:when test="assignments:value='2101'">,	950,	</xsl:when>
			<xsl:when test="assignments:value='2108'">,	51932,	</xsl:when>
			<xsl:when test="assignments:value='2109'">,	874,	</xsl:when>
			<xsl:when test="assignments:value='2250'">,	1250,	</xsl:when>
			<xsl:when test="assignments:value='2251'">,	1251,	</xsl:when>
			<xsl:when test="assignments:value='2252'">,	1252,	</xsl:when>
			<xsl:when test="assignments:value='2253'">,	1253,	</xsl:when>
			<xsl:when test="assignments:value='2254'">,	1254,	</xsl:when>
			<xsl:when test="assignments:value='2255'">,	1255,	</xsl:when>
			<xsl:when test="assignments:value='2256'">,	1256,	</xsl:when>
			<xsl:when test="assignments:value='2257'">,	1257,	</xsl:when>
			<xsl:when test="assignments:value='2258'">,	1258,	</xsl:when>
			<xsl:when test="assignments:value='2259'">,	874,	</xsl:when>
			<xsl:when test="assignments:value='2260'">,	50220,	</xsl:when>
			<xsl:when test="assignments:value=''">,	,	</xsl:when>
			<xsl:when test="assignments:value=''">,	,	</xsl:when>
			<xsl:otherwise>,	0,		</xsl:otherwise>
		</xsl:choose>
		<xsl:text>&quot;</xsl:text>
		<xsl:choose>
			<xsl:when test="assignments:preferred_alias">
				<xsl:value-of select="translate(assignments:preferred_alias, $upr, $lwr)"/>
			</xsl:when>
			<xsl:otherwise>
				<xsl:value-of select="translate(assignments:name, $upr, $lwr)"/>
			</xsl:otherwise>
		</xsl:choose>
		<xsl:text>&quot;)&#xa;</xsl:text>
	</xsl:template>
	<xsl:template match="*">
	</xsl:template>
</xsl:stylesheet>