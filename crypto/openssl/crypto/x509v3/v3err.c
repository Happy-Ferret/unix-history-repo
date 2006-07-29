/* crypto/x509v3/v3err.c */
/* ====================================================================
 * Copyright (c) 1999-2005 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.OpenSSL.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    openssl-core@OpenSSL.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.OpenSSL.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com).
 *
 */

/* NOTE: this file was auto generated by the mkerr.pl script: any changes
 * made to it will be overwritten when the script next updates this file,
 * only reason strings will be preserved.
 */

#include <stdio.h>
#include <openssl/err.h>
#include <openssl/x509v3.h>

/* BEGIN ERROR CODES */
#ifndef OPENSSL_NO_ERR

#define ERR_FUNC(func) ERR_PACK(ERR_LIB_X509V3,func,0)
#define ERR_REASON(reason) ERR_PACK(ERR_LIB_X509V3,0,reason)

static ERR_STRING_DATA X509V3_str_functs[]=
	{
{ERR_FUNC(X509V3_F_COPY_EMAIL),	"COPY_EMAIL"},
{ERR_FUNC(X509V3_F_COPY_ISSUER),	"COPY_ISSUER"},
{ERR_FUNC(X509V3_F_DO_DIRNAME),	"DO_DIRNAME"},
{ERR_FUNC(X509V3_F_DO_EXT_CONF),	"DO_EXT_CONF"},
{ERR_FUNC(X509V3_F_DO_EXT_I2D),	"DO_EXT_I2D"},
{ERR_FUNC(X509V3_F_DO_EXT_NCONF),	"DO_EXT_NCONF"},
{ERR_FUNC(X509V3_F_DO_I2V_NAME_CONSTRAINTS),	"DO_I2V_NAME_CONSTRAINTS"},
{ERR_FUNC(X509V3_F_HEX_TO_STRING),	"hex_to_string"},
{ERR_FUNC(X509V3_F_I2S_ASN1_ENUMERATED),	"i2s_ASN1_ENUMERATED"},
{ERR_FUNC(X509V3_F_I2S_ASN1_IA5STRING),	"I2S_ASN1_IA5STRING"},
{ERR_FUNC(X509V3_F_I2S_ASN1_INTEGER),	"i2s_ASN1_INTEGER"},
{ERR_FUNC(X509V3_F_I2V_AUTHORITY_INFO_ACCESS),	"I2V_AUTHORITY_INFO_ACCESS"},
{ERR_FUNC(X509V3_F_NOTICE_SECTION),	"NOTICE_SECTION"},
{ERR_FUNC(X509V3_F_NREF_NOS),	"NREF_NOS"},
{ERR_FUNC(X509V3_F_POLICY_SECTION),	"POLICY_SECTION"},
{ERR_FUNC(X509V3_F_PROCESS_PCI_VALUE),	"PROCESS_PCI_VALUE"},
{ERR_FUNC(X509V3_F_R2I_CERTPOL),	"R2I_CERTPOL"},
{ERR_FUNC(X509V3_F_R2I_PCI),	"R2I_PCI"},
{ERR_FUNC(X509V3_F_S2I_ASN1_IA5STRING),	"S2I_ASN1_IA5STRING"},
{ERR_FUNC(X509V3_F_S2I_ASN1_INTEGER),	"s2i_ASN1_INTEGER"},
{ERR_FUNC(X509V3_F_S2I_ASN1_OCTET_STRING),	"s2i_ASN1_OCTET_STRING"},
{ERR_FUNC(X509V3_F_S2I_ASN1_SKEY_ID),	"S2I_ASN1_SKEY_ID"},
{ERR_FUNC(X509V3_F_S2I_SKEY_ID),	"S2I_SKEY_ID"},
{ERR_FUNC(X509V3_F_STRING_TO_HEX),	"string_to_hex"},
{ERR_FUNC(X509V3_F_SXNET_ADD_ID_ASC),	"SXNET_ADD_ID_ASC"},
{ERR_FUNC(X509V3_F_SXNET_ADD_ID_INTEGER),	"SXNET_add_id_INTEGER"},
{ERR_FUNC(X509V3_F_SXNET_ADD_ID_ULONG),	"SXNET_add_id_ulong"},
{ERR_FUNC(X509V3_F_SXNET_GET_ID_ASC),	"SXNET_get_id_asc"},
{ERR_FUNC(X509V3_F_SXNET_GET_ID_ULONG),	"SXNET_get_id_ulong"},
{ERR_FUNC(X509V3_F_V2I_ASN1_BIT_STRING),	"V2I_ASN1_BIT_STRING"},
{ERR_FUNC(X509V3_F_V2I_AUTHORITY_INFO_ACCESS),	"V2I_AUTHORITY_INFO_ACCESS"},
{ERR_FUNC(X509V3_F_V2I_AUTHORITY_KEYID),	"V2I_AUTHORITY_KEYID"},
{ERR_FUNC(X509V3_F_V2I_BASIC_CONSTRAINTS),	"V2I_BASIC_CONSTRAINTS"},
{ERR_FUNC(X509V3_F_V2I_CRLD),	"V2I_CRLD"},
{ERR_FUNC(X509V3_F_V2I_EXTENDED_KEY_USAGE),	"V2I_EXTENDED_KEY_USAGE"},
{ERR_FUNC(X509V3_F_V2I_GENERAL_NAMES),	"v2i_GENERAL_NAMES"},
{ERR_FUNC(X509V3_F_V2I_GENERAL_NAME_EX),	"v2i_GENERAL_NAME_ex"},
{ERR_FUNC(X509V3_F_V2I_ISSUER_ALT),	"V2I_ISSUER_ALT"},
{ERR_FUNC(X509V3_F_V2I_NAME_CONSTRAINTS),	"V2I_NAME_CONSTRAINTS"},
{ERR_FUNC(X509V3_F_V2I_POLICY_CONSTRAINTS),	"V2I_POLICY_CONSTRAINTS"},
{ERR_FUNC(X509V3_F_V2I_POLICY_MAPPINGS),	"V2I_POLICY_MAPPINGS"},
{ERR_FUNC(X509V3_F_V2I_SUBJECT_ALT),	"V2I_SUBJECT_ALT"},
{ERR_FUNC(X509V3_F_V3_GENERIC_EXTENSION),	"V3_GENERIC_EXTENSION"},
{ERR_FUNC(X509V3_F_X509V3_ADD1_I2D),	"X509V3_add1_i2d"},
{ERR_FUNC(X509V3_F_X509V3_ADD_VALUE),	"X509V3_add_value"},
{ERR_FUNC(X509V3_F_X509V3_EXT_ADD),	"X509V3_EXT_add"},
{ERR_FUNC(X509V3_F_X509V3_EXT_ADD_ALIAS),	"X509V3_EXT_add_alias"},
{ERR_FUNC(X509V3_F_X509V3_EXT_CONF),	"X509V3_EXT_conf"},
{ERR_FUNC(X509V3_F_X509V3_EXT_I2D),	"X509V3_EXT_i2d"},
{ERR_FUNC(X509V3_F_X509V3_EXT_NCONF),	"X509V3_EXT_nconf"},
{ERR_FUNC(X509V3_F_X509V3_GET_SECTION),	"X509V3_GET_SECTION"},
{ERR_FUNC(X509V3_F_X509V3_GET_STRING),	"X509V3_get_string"},
{ERR_FUNC(X509V3_F_X509V3_GET_VALUE_BOOL),	"X509V3_get_value_bool"},
{ERR_FUNC(X509V3_F_X509V3_PARSE_LIST),	"X509V3_PARSE_LIST"},
{ERR_FUNC(X509V3_F_X509_PURPOSE_ADD),	"X509_PURPOSE_add"},
{ERR_FUNC(X509V3_F_X509_PURPOSE_SET),	"X509_PURPOSE_set"},
{0,NULL}
	};

static ERR_STRING_DATA X509V3_str_reasons[]=
	{
{ERR_REASON(X509V3_R_BAD_IP_ADDRESS)     ,"bad ip address"},
{ERR_REASON(X509V3_R_BAD_OBJECT)         ,"bad object"},
{ERR_REASON(X509V3_R_BN_DEC2BN_ERROR)    ,"bn dec2bn error"},
{ERR_REASON(X509V3_R_BN_TO_ASN1_INTEGER_ERROR),"bn to asn1 integer error"},
{ERR_REASON(X509V3_R_DIRNAME_ERROR)      ,"dirname error"},
{ERR_REASON(X509V3_R_DUPLICATE_ZONE_ID)  ,"duplicate zone id"},
{ERR_REASON(X509V3_R_ERROR_CONVERTING_ZONE),"error converting zone"},
{ERR_REASON(X509V3_R_ERROR_CREATING_EXTENSION),"error creating extension"},
{ERR_REASON(X509V3_R_ERROR_IN_EXTENSION) ,"error in extension"},
{ERR_REASON(X509V3_R_EXPECTED_A_SECTION_NAME),"expected a section name"},
{ERR_REASON(X509V3_R_EXTENSION_EXISTS)   ,"extension exists"},
{ERR_REASON(X509V3_R_EXTENSION_NAME_ERROR),"extension name error"},
{ERR_REASON(X509V3_R_EXTENSION_NOT_FOUND),"extension not found"},
{ERR_REASON(X509V3_R_EXTENSION_SETTING_NOT_SUPPORTED),"extension setting not supported"},
{ERR_REASON(X509V3_R_EXTENSION_VALUE_ERROR),"extension value error"},
{ERR_REASON(X509V3_R_ILLEGAL_EMPTY_EXTENSION),"illegal empty extension"},
{ERR_REASON(X509V3_R_ILLEGAL_HEX_DIGIT)  ,"illegal hex digit"},
{ERR_REASON(X509V3_R_INCORRECT_POLICY_SYNTAX_TAG),"incorrect policy syntax tag"},
{ERR_REASON(X509V3_R_INVALID_BOOLEAN_STRING),"invalid boolean string"},
{ERR_REASON(X509V3_R_INVALID_EXTENSION_STRING),"invalid extension string"},
{ERR_REASON(X509V3_R_INVALID_NAME)       ,"invalid name"},
{ERR_REASON(X509V3_R_INVALID_NULL_ARGUMENT),"invalid null argument"},
{ERR_REASON(X509V3_R_INVALID_NULL_NAME)  ,"invalid null name"},
{ERR_REASON(X509V3_R_INVALID_NULL_VALUE) ,"invalid null value"},
{ERR_REASON(X509V3_R_INVALID_NUMBER)     ,"invalid number"},
{ERR_REASON(X509V3_R_INVALID_NUMBERS)    ,"invalid numbers"},
{ERR_REASON(X509V3_R_INVALID_OBJECT_IDENTIFIER),"invalid object identifier"},
{ERR_REASON(X509V3_R_INVALID_OPTION)     ,"invalid option"},
{ERR_REASON(X509V3_R_INVALID_POLICY_IDENTIFIER),"invalid policy identifier"},
{ERR_REASON(X509V3_R_INVALID_PROXY_POLICY_SETTING),"invalid proxy policy setting"},
{ERR_REASON(X509V3_R_INVALID_PURPOSE)    ,"invalid purpose"},
{ERR_REASON(X509V3_R_INVALID_SECTION)    ,"invalid section"},
{ERR_REASON(X509V3_R_INVALID_SYNTAX)     ,"invalid syntax"},
{ERR_REASON(X509V3_R_ISSUER_DECODE_ERROR),"issuer decode error"},
{ERR_REASON(X509V3_R_MISSING_VALUE)      ,"missing value"},
{ERR_REASON(X509V3_R_NEED_ORGANIZATION_AND_NUMBERS),"need organization and numbers"},
{ERR_REASON(X509V3_R_NO_CONFIG_DATABASE) ,"no config database"},
{ERR_REASON(X509V3_R_NO_ISSUER_CERTIFICATE),"no issuer certificate"},
{ERR_REASON(X509V3_R_NO_ISSUER_DETAILS)  ,"no issuer details"},
{ERR_REASON(X509V3_R_NO_POLICY_IDENTIFIER),"no policy identifier"},
{ERR_REASON(X509V3_R_NO_PROXY_CERT_POLICY_LANGUAGE_DEFINED),"no proxy cert policy language defined"},
{ERR_REASON(X509V3_R_NO_PUBLIC_KEY)      ,"no public key"},
{ERR_REASON(X509V3_R_NO_SUBJECT_DETAILS) ,"no subject details"},
{ERR_REASON(X509V3_R_ODD_NUMBER_OF_DIGITS),"odd number of digits"},
{ERR_REASON(X509V3_R_OPERATION_NOT_DEFINED),"operation not defined"},
{ERR_REASON(X509V3_R_OTHERNAME_ERROR)    ,"othername error"},
{ERR_REASON(X509V3_R_POLICY_LANGUAGE_ALREADTY_DEFINED),"policy language alreadty defined"},
{ERR_REASON(X509V3_R_POLICY_PATH_LENGTH) ,"policy path length"},
{ERR_REASON(X509V3_R_POLICY_PATH_LENGTH_ALREADTY_DEFINED),"policy path length alreadty defined"},
{ERR_REASON(X509V3_R_POLICY_SYNTAX_NOT_CURRENTLY_SUPPORTED),"policy syntax not currently supported"},
{ERR_REASON(X509V3_R_POLICY_WHEN_PROXY_LANGUAGE_REQUIRES_NO_POLICY),"policy when proxy language requires no policy"},
{ERR_REASON(X509V3_R_SECTION_NOT_FOUND)  ,"section not found"},
{ERR_REASON(X509V3_R_UNABLE_TO_GET_ISSUER_DETAILS),"unable to get issuer details"},
{ERR_REASON(X509V3_R_UNABLE_TO_GET_ISSUER_KEYID),"unable to get issuer keyid"},
{ERR_REASON(X509V3_R_UNKNOWN_BIT_STRING_ARGUMENT),"unknown bit string argument"},
{ERR_REASON(X509V3_R_UNKNOWN_EXTENSION)  ,"unknown extension"},
{ERR_REASON(X509V3_R_UNKNOWN_EXTENSION_NAME),"unknown extension name"},
{ERR_REASON(X509V3_R_UNKNOWN_OPTION)     ,"unknown option"},
{ERR_REASON(X509V3_R_UNSUPPORTED_OPTION) ,"unsupported option"},
{ERR_REASON(X509V3_R_USER_TOO_LONG)      ,"user too long"},
{0,NULL}
	};

#endif

void ERR_load_X509V3_strings(void)
	{
	static int init=1;

	if (init)
		{
		init=0;
#ifndef OPENSSL_NO_ERR
		ERR_load_strings(0,X509V3_str_functs);
		ERR_load_strings(0,X509V3_str_reasons);
#endif

		}
	}
