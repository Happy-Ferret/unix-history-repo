#!/usr/local/bin/perl

print "/* apps/progs.h */\n";
print "/* automatically generated by progs.pl for openssl.c */\n\n";

grep(s/^asn1pars$/asn1parse/,@ARGV);

foreach (@ARGV)
	{ printf "extern int %s_main(int argc,char *argv[]);\n",$_; }

print <<'EOF';

#define FUNC_TYPE_GENERAL	1
#define FUNC_TYPE_MD		2
#define FUNC_TYPE_CIPHER	3
#define FUNC_TYPE_PKEY		4
#define FUNC_TYPE_MD_ALG	5
#define FUNC_TYPE_CIPHER_ALG	6

typedef struct {
	int type;
	const char *name;
	int (*func)(int argc,char *argv[]);
	} FUNCTION;
DECLARE_LHASH_OF(FUNCTION);

FUNCTION functions[] = {
EOF

foreach (@ARGV)
	{
	push(@files,$_);
	$str="\t{FUNC_TYPE_GENERAL,\"$_\",${_}_main},\n";
	if (($_ =~ /^s_/) || ($_ =~ /^ciphers$/))
		{ print "#if !defined(OPENSSL_NO_SOCK) && !(defined(OPENSSL_NO_SSL2) && defined(OPENSSL_NO_SSL3))\n${str}#endif\n"; } 
	elsif ( ($_ =~ /^speed$/))
		{ print "#ifndef OPENSSL_NO_SPEED\n${str}#endif\n"; }
	elsif ( ($_ =~ /^engine$/))
		{ print "#ifndef OPENSSL_NO_ENGINE\n${str}#endif\n"; }
	elsif ( ($_ =~ /^rsa$/) || ($_ =~ /^genrsa$/) || ($_ =~ /^rsautl$/)) 
		{ print "#ifndef OPENSSL_NO_RSA\n${str}#endif\n";  }
	elsif ( ($_ =~ /^dsa$/) || ($_ =~ /^gendsa$/) || ($_ =~ /^dsaparam$/))
		{ print "#ifndef OPENSSL_NO_DSA\n${str}#endif\n"; }
	elsif ( ($_ =~ /^ec$/) || ($_ =~ /^ecparam$/))
		{ print "#ifndef OPENSSL_NO_EC\n${str}#endif\n";}
	elsif ( ($_ =~ /^dh$/) || ($_ =~ /^gendh$/) || ($_ =~ /^dhparam$/))
		{ print "#ifndef OPENSSL_NO_DH\n${str}#endif\n"; }
	elsif ( ($_ =~ /^pkcs12$/))
		{ print "#if !defined(OPENSSL_NO_DES) && !defined(OPENSSL_NO_SHA1)\n${str}#endif\n"; }
	elsif ( ($_ =~ /^cms$/))
		{ print "#ifndef OPENSSL_NO_CMS\n${str}#endif\n"; }
	elsif ( ($_ =~ /^ocsp$/))
		{ print "#ifndef OPENSSL_NO_OCSP\n${str}#endif\n"; }
	elsif ( ($_ =~ /^srp$/))
		{ print "#ifndef OPENSSL_NO_SRP\n${str}#endif\n"; }
	else
		{ print $str; }
	}

foreach ("md2","md4","md5","sha","sha1","mdc2","rmd160")
	{
	push(@files,$_);
	printf "#ifndef OPENSSL_NO_".uc($_)."\n\t{FUNC_TYPE_MD,\"".$_."\",dgst_main},\n#endif\n";
	}

foreach (
	"aes-128-cbc", "aes-128-ecb",
	"aes-192-cbc", "aes-192-ecb",
	"aes-256-cbc", "aes-256-ecb",
	"camellia-128-cbc", "camellia-128-ecb",
	"camellia-192-cbc", "camellia-192-ecb",
	"camellia-256-cbc", "camellia-256-ecb",
	"base64", "zlib",
	"des", "des3", "desx", "idea", "seed", "rc4", "rc4-40",
	"rc2", "bf", "cast", "rc5",
	"des-ecb", "des-ede",    "des-ede3",
	"des-cbc", "des-ede-cbc","des-ede3-cbc",
	"des-cfb", "des-ede-cfb","des-ede3-cfb",
	"des-ofb", "des-ede-ofb","des-ede3-ofb",
	"idea-cbc","idea-ecb",    "idea-cfb", "idea-ofb",
	"seed-cbc","seed-ecb",    "seed-cfb", "seed-ofb",
	"rc2-cbc", "rc2-ecb", "rc2-cfb","rc2-ofb", "rc2-64-cbc", "rc2-40-cbc",
	"bf-cbc",  "bf-ecb",     "bf-cfb",   "bf-ofb",
	"cast5-cbc","cast5-ecb", "cast5-cfb","cast5-ofb",
	"cast-cbc", "rc5-cbc",   "rc5-ecb",  "rc5-cfb",  "rc5-ofb")
	{
	push(@files,$_);

	$t=sprintf("\t{FUNC_TYPE_CIPHER,\"%s\",enc_main},\n",$_);
	if    ($_ =~ /des/)  { $t="#ifndef OPENSSL_NO_DES\n${t}#endif\n"; }
	elsif ($_ =~ /aes/)  { $t="#ifndef OPENSSL_NO_AES\n${t}#endif\n"; }
	elsif ($_ =~ /camellia/)  { $t="#ifndef OPENSSL_NO_CAMELLIA\n${t}#endif\n"; }
	elsif ($_ =~ /idea/) { $t="#ifndef OPENSSL_NO_IDEA\n${t}#endif\n"; }
	elsif ($_ =~ /seed/) { $t="#ifndef OPENSSL_NO_SEED\n${t}#endif\n"; }
	elsif ($_ =~ /rc4/)  { $t="#ifndef OPENSSL_NO_RC4\n${t}#endif\n"; }
	elsif ($_ =~ /rc2/)  { $t="#ifndef OPENSSL_NO_RC2\n${t}#endif\n"; }
	elsif ($_ =~ /bf/)   { $t="#ifndef OPENSSL_NO_BF\n${t}#endif\n"; }
	elsif ($_ =~ /cast/) { $t="#ifndef OPENSSL_NO_CAST\n${t}#endif\n"; }
	elsif ($_ =~ /rc5/)  { $t="#ifndef OPENSSL_NO_RC5\n${t}#endif\n"; }
	elsif ($_ =~ /zlib/)  { $t="#ifdef ZLIB\n${t}#endif\n"; }
	print $t;
	}

print "\t{0,NULL,NULL}\n\t};\n";
