#Copyright 2008,2009,2010 Alun Williams
#This file is part of MAF.
#MAF is free software: you can redistribute it and/or modify it
#under the terms of the GNU General Public License as published by
#the Free Software Foundation, either version 3 of the License, or
#(at your option) any later version.

#MAF is distributed in the hope that it will be useful,
#but WITHOUT ANY WARRANTY; without even the implied warranty of
#MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#GNU General Public License for more details.
#You should have received a copy of the GNU General Public License
#along with MAF.  If not, see <http://www.gnu.org/licenses/>.

#
#$Log: maf.mak $
#Revision 1.25  2011/06/07 11:32:02Z  Alun
#added gpxlatwa to build
#Revision 1.24  2011/05/27 14:47:52Z  Alun
#Added fsaseparate utility
#Revision 1.23  2010/07/04 09:02:02Z  Alun
#Added maf.rbj to maf.dll
#Revision 1.22  2010/05/18 10:42:56Z  Alun
#Jun 2010 version. Many new utilities added
#Revision 1.21  2009/11/10 19:26:28Z  Alun
#Added gpcclass
#Revision 1.20  2009/10/14 00:04:57Z  Alun
#mafu64.mak had got lost from unmake stuff!
#Revision 1.19  2009/10/07 08:17:14Z  Alun
#Added fsatrim to build
#Revision 1.18  2009/09/14 10:42:31Z  Alun
#Additional source code changes needed to get clean compilation on latest GNU
#Revision 1.17  2009/09/13 20:53:20Z  Alun
#Added mafu64.mak. Changed linker flags slightly
#Revision 1.16  2009/09/02 06:59:12Z  Alun
#Removed delay loading of maf.dll which is pointless and a relic of an attempt
#to debug load-time problems that no longer exist
#Revision 1.15  2009/08/03 22:03:48Z  Alun
#New utilities added to build
#Revision 1.17  2008/11/04 23:49:53Z  Alun
#Extra stuff to get Unix files into rcs was being ignored by unmake
#Revision 1.16  2008/11/04 23:29:46Z  Alun
#Another attempt to get unmake to deal with Unix files
#Revision 1.15  2008/11/04 22:55:58Z  Alun
#Early November checkin
#Completely reworked pool and Difference_Tracker
#Ported to Darwin
#Revision 1.14  2008/10/10 07:49:22Z  Alun
#New utilities added
#Revision 1.13  2008/09/30 10:13:12Z  Alun
#Switch to using Hash+Collection_Manager as replacement for Indexer.
#Currently this is about 10% slower, but this is a more flexible
#architecture.
#Revision 1.7  2007/12/20 23:25:45Z  Alun
#

#L32EXE = -debug -pdb:none -nod -opt:ref -entry:mainCRTStartup -stack:0x600000,0x100000 libc.lib kernel32.lib user32.lib 
L32EXE = -nologo -nod -opt:ref -entry:mainCRTStartup -stack:0x600000,0x100000 -opt:NOWIN98 kernel32.lib user32.lib 
#L32DLL= -debug -pdb:none -dll -nod -nod:libm.lib -entry:_DllMainCRTStartup@12 -subsystem:windows,4.0 libc.lib kernel32.lib -out:$@ 
L32DLL= -nologo -nod -dll -opt:NOWIN98 -entry:_DllMainCRTStartup@12 -subsystem:windows,4.0 libc.lib kernel32.lib -out:$@ 
#The definitions below are commented out, because they are in the tools.ini file
#By default MAF will be built as a fully optimised with no debug information
#The command nmake -f maf.mak DEBUG=1 will build a debug build.
#In this case DEBUG is still not defined on the command line of the compiler
#You may wish to add this, as it will enable various extra checks internally
#and add extra information to various structures which makes using a 
#visual debugger easier.
#You may find it convenient to uncomment and alter some of the definitions below
#if debugging.
#PFLAGS = -DWIN32  -D_WIN32_WINDOWS=0x501 -D_WIN32_WINNT=0x501 -DWIN32_LEAN_AND_MEAN -D_X86_
#COMMON = -nologo -c -I. -J -GB -Gy -W4 -Fo$@ -Od -Zi
#CFLAGS = $(COMMON) -Za
#CWFLAGS = $(COMMON) $(PFLAGS) -Ze
BIN= ./../bin

all: \
  $(BIN)/maf.dll \
  $(BIN)/example.exe \
  $(BIN)/automata.exe \
  $(BIN)/autcos.exe \
  $(BIN)/autgroup.exe \
  $(BIN)/kbprog.exe \
  $(BIN)/kbprogcos.exe \
  $(BIN)/fsaand.exe \
  $(BIN)/fsaandnot.exe \
  $(BIN)/fsabfs.exe \
  $(BIN)/fsacartesian.exe \
  $(BIN)/fsacompose.exe \
  $(BIN)/fsaconcat.exe \
  $(BIN)/fsacount.exe \
  $(BIN)/fsacut.exe \
  $(BIN)/fsadiagonal.exe \
  $(BIN)/fsaenumerate.exe \
  $(BIN)/fsaexists.exe \
  $(BIN)/fsafl.exe \
  $(BIN)/fsakernel.exe \
  $(BIN)/fsalequal.exe \
  $(BIN)/fsamerge.exe \
  $(BIN)/fsamin.exe \
  $(BIN)/fsatrim.exe \
  $(BIN)/fsanot.exe \
  $(BIN)/fsaor.exe \
  $(BIN)/fsaprint.exe \
  $(BIN)/fsaproduct.exe \
  $(BIN)/fsaprune.exe \
  $(BIN)/fsaread.exe \
  $(BIN)/fsareverse.exe \
  $(BIN)/fsan.exe \
  $(BIN)/fsapad.exe \
  $(BIN)/fsaseparate.exe \
  $(BIN)/fsashortlex.exe \
  $(BIN)/fsastar.exe \
  $(BIN)/fsaswapcoords.exe \
  $(BIN)/gpaxioms.exe \
  $(BIN)/gpcclass.exe \
  $(BIN)/gpcosets.exe \
  $(BIN)/gpdifflabs.exe \
  $(BIN)/gpgenmult2.exe \
  $(BIN)/gpgeowa.exe \
  $(BIN)/gpmakefsa.exe \
  $(BIN)/gpmigmdet.exe \
  $(BIN)/gpmigenmult2.exe \
  $(BIN)/gpmimult.exe \
  $(BIN)/gpmimult2.exe \
  $(BIN)/gpminkb.exe \
  $(BIN)/gpmorphism.exe \
  $(BIN)/gpmult.exe \
  $(BIN)/gpmult2.exe \
  $(BIN)/gpovlwa.exe \
  $(BIN)/gpstabiliser.exe \
  $(BIN)/gpsublowindex.exe \
  $(BIN)/gpsubmake.exe \
  $(BIN)/gpsubpres.exe \
  $(BIN)/gpsubwa.exe \
  $(BIN)/gptcenum.exe \
  $(BIN)/gpvital.exe \
  $(BIN)/gpwa.exe \
  $(BIN)/gpxlatwa.exe \
  $(BIN)/midfadeterminize.exe \
  $(BIN)/reduce.exe \
  $(BIN)/gporder.exe \
  $(BIN)/rwsprint.exe \
  $(BIN)/makecosfile.exe  \
  $(BIN)/isconjugate.exe \
  $(BIN)/isnormal.exe \
  $(BIN)/simplify.exe \
  dummy

clean:
	del *.o32 *.v32 *.rbj *.pdb *.opt

MAFLIB = \
  mafbase.$O \
  container.$O \
  platform.$W \
  stream.$W \
  maf.$O \
  present.$O \
  equation.$O \
  mafload.$O \
  mafread.$O \
  alphabet.$O \
  maf_avl.$O \
  maf_cfp.$O \
  maf_dr.$O \
  maf_dt.$O \
  maf_el.$O \
  maf_em.$O \
  maf_ew.$O \
  maf_jm.$O \
  maf_mult.$O \
  maf_nm.$O \
  maf_rm.$O \
  maf_rws.$O \
  maf_spl.$O \
  maf_so.$O \
  maf_ss.$O \
  maf_sub.$O \
  maf_subwa.$O \
  maf_we.$O \
  mafqueue.$O \
  mafctype.$O \
  mafword.$O \
  hash.$O \
  keyedfsa.$O \
  bitarray.$O \
  heap.$W \
  rubik.$O \
  fsa.$O \
  mafauto.$O  \
  mafconj.$O \
  mafcoset.$O \
  mafgeowa.$O \
  mafminkb.$O \
  nodelist.$O \
  mafnode.$O \
  ltfsa.$O \
  maf_tc.$O \
  relators.$O \
  lowindex.$O \
  subpres.$O \
  tietze.$O 
   
LIBS = startup.lib maf.lib

FSAREAD_EXE = \
  fsaread.$O \
  fsaread.rbj \
  $(LIBS)

$(BIN)/fsaread.exe: $(FSAREAD_EXE) 
	$(LINK32) $(FSAREAD_EXE) $(L32EXE) -out:$@ -map:$M 

REDUCE_EXE = \
  reduce.$O \
  reduce.rbj \
  $(LIBS)

$(BIN)/reduce.exe: $(REDUCE_EXE) 
	$(LINK32) $(REDUCE_EXE) $(L32EXE) -out:$@ -map:$M 

GPORDER_EXE = \
  gporder.$O \
  gporder.rbj \
  $(LIBS)

$(BIN)/gporder.exe: $(GPORDER_EXE) 
	$(LINK32) $(GPORDER_EXE) $(L32EXE) -out:$@ -map:$M 


EXAMPLE_EXE = \
  example.$O \
  example.rbj \
  $(LIBS)

$(BIN)/example.exe: $(EXAMPLE_EXE) 
	$(LINK32) $(EXAMPLE_EXE) $(L32EXE) -out:$@ -map:$M 

AUTCOS_EXE = \
  autcos.$O \
  autcos.rbj \
  $(LIBS)  

$(BIN)/autcos.exe: $(AUTCOS_EXE) 
	$(LINK32) $(AUTCOS_EXE) $(L32EXE) -out:$@ -map:$M 

AUTGROUP_EXE = \
  autgroup.$O \
  autgroup.rbj \
  $(LIBS)

$(BIN)/autgroup.exe: $(AUTGROUP_EXE) 
	$(LINK32) $(AUTGROUP_EXE) $(L32EXE) -out:$@ -map:$M 

AUTOMATA_EXE = \
  automata.$O \
  automata.rbj \
  $(LIBS)

$(BIN)/automata.exe: $(AUTOMATA_EXE) 
	$(LINK32) $(AUTOMATA_EXE) $(L32EXE) -out:$@ -map:$M 

KBPROG_EXE =  \
  kbprog.$O \
  kbprog.rbj \
  $(LIBS)

$(BIN)/kbprog.exe: $(KBPROG_EXE) 
	$(LINK32) $(KBPROG_EXE) $(L32EXE) -out:$@ -map:$M 

KBPROGCOS_EXE =  \
  kbprogcos.$O \
  kbprogcos.rbj \
  $(LIBS)

$(BIN)/kbprogcos.exe: $(KBPROGCOS_EXE) 
	$(LINK32) $(KBPROGCOS_EXE) $(L32EXE) -out:$@ -map:$M 

FSACONCAT_EXE = \
  fsaconcat.$O \
  fsaconcat.rbj \
  $(LIBS)

$(BIN)/fsaconcat.exe: $(FSACONCAT_EXE) 
	$(LINK32) $(FSACONCAT_EXE) $(L32EXE) -out:$@ -map:$M 

FSAPRODUCT_EXE = \
  fsaproduct.$O \
  fsaproduct.rbj \
  $(LIBS)

$(BIN)/fsaproduct.exe: $(FSAPRODUCT_EXE) 
	$(LINK32) $(FSAPRODUCT_EXE) $(L32EXE) -out:$@ -map:$M 

FSACOUNT_EXE = \
  fsacount.$O \
  fsacount.rbj \
  $(LIBS)

$(BIN)/fsacount.exe: $(FSACOUNT_EXE) 
	$(LINK32) $(FSACOUNT_EXE) $(L32EXE) -out:$@ -map:$M 

FSACUT_EXE = \
  fsacut.$O \
  fsacut.rbj \
  $(LIBS)

$(BIN)/fsacut.exe: $(FSACUT_EXE) 
	$(LINK32) $(FSACUT_EXE) $(L32EXE) -out:$@ -map:$M 

FSAPRINT_EXE = \
  fsaprint.$O \
  fsaprint.rbj \
  $(LIBS)

$(BIN)/fsaprint.exe: $(FSAPRINT_EXE) 
	$(LINK32) $(FSAPRINT_EXE) $(L32EXE) -out:$@ -map:$M 

FSAAND_EXE = \
  fsaand.$O \
  fsaand.rbj \
  $(LIBS)

$(BIN)/fsaand.exe: $(FSAAND_EXE) 
	$(LINK32) $(FSAAND_EXE) $(L32EXE) -out:$@ -map:$M 

FSAOR_EXE = \
  fsaor.$O \
  fsaor.rbj \
  $(LIBS)

$(BIN)/fsaor.exe: $(FSAOR_EXE) 
	$(LINK32) $(FSAOR_EXE) $(L32EXE) -out:$@ -map:$M 

FSAANDNOT_EXE = \
  fsaandnot.$O \
  fsaandnot.rbj \
  $(LIBS)

$(BIN)/fsaandnot.exe: $(FSAANDNOT_EXE) 
	$(LINK32) $(FSAANDNOT_EXE) $(L32EXE) -out:$@ -map:$M 

FSABFS_EXE = \
  fsabfs.$O \
  fsabfs.rbj \
  $(LIBS)

$(BIN)/fsabfs.exe: $(FSABFS_EXE) 
	$(LINK32) $(FSABFS_EXE) $(L32EXE) -out:$@ -map:$M 

FSADIAGONAL_EXE = \
  fsadiagonal.$O \
  fsadiagonal.rbj \
  $(LIBS)

$(BIN)/fsadiagonal.exe: $(FSADIAGONAL_EXE) 
	$(LINK32) $(FSADIAGONAL_EXE) $(L32EXE) -out:$@ -map:$M 


FSAMERGE_EXE = \
  fsamerge.$O \
  fsamerge.rbj \
  $(LIBS)

$(BIN)/fsamerge.exe: $(FSAMERGE_EXE) 
	$(LINK32) $(FSAMERGE_EXE) $(L32EXE) -out:$@ -map:$M 

FSAMIN_EXE = \
  fsamin.$O \
  fsamin.rbj \
  $(LIBS)

$(BIN)/fsamin.exe: $(FSAMIN_EXE) 
	$(LINK32) $(FSAMIN_EXE) $(L32EXE) -out:$@ -map:$M 

FSATRIM_EXE = \
  fsatrim.$O \
  fsatrim.rbj \
  $(LIBS)

$(BIN)/fsatrim.exe: $(FSATRIM_EXE) 
	$(LINK32) $(FSATRIM_EXE) $(L32EXE) -out:$@ -map:$M 

FSAPRUNE_EXE = \
  fsaprune.$O \
  fsaprune.rbj \
  $(LIBS)

$(BIN)/fsaprune.exe: $(FSAPRUNE_EXE) 
	$(LINK32) $(FSAPRUNE_EXE) $(L32EXE) -out:$@ -map:$M 

FSAKERNEL_EXE = \
  fsakernel.$O \
  fsakernel.rbj \
  $(LIBS)

$(BIN)/fsakernel.exe: $(FSAKERNEL_EXE) 
	$(LINK32) $(FSAKERNEL_EXE) $(L32EXE) -out:$@ -map:$M 

FSAENUMERATE_EXE = \
  fsaenumerate.$O \
  fsaenumerate.rbj \
  $(LIBS)

$(BIN)/fsaenumerate.exe: $(FSAENUMERATE_EXE) 
	$(LINK32) $(FSAENUMERATE_EXE) $(L32EXE) -out:$@ -map:$M 

FSAEXISTS_EXE = \
  fsaexists.$O \
  fsaexists.rbj \
  $(LIBS)

$(BIN)/fsaexists.exe: $(FSAEXISTS_EXE) 
	$(LINK32) $(FSAEXISTS_EXE) $(L32EXE) -out:$@ -map:$M 

FSAFL_EXE = \
  fsafl.$O \
  fsafl.rbj \
  $(LIBS)

$(BIN)/fsafl.exe: $(FSAFL_EXE) 
	$(LINK32) $(FSAFL_EXE) $(L32EXE) -out:$@ -map:$M 

FSAN_EXE = \
  fsan.$O \
  fsan.rbj \
  $(LIBS)

$(BIN)/fsan.exe: $(FSAN_EXE) 
	$(LINK32) $(FSAN_EXE) $(L32EXE) -out:$@ -map:$M 

FSAPAD_EXE = \
  fsapad.$O \
  fsapad.rbj \
  $(LIBS)

$(BIN)/fsapad.exe: $(FSAPAD_EXE) 
	$(LINK32) $(FSAPAD_EXE) $(L32EXE) -out:$@ -map:$M 

FSASHORTLEX_EXE = \
  fsashortlex.$O \
  fsashortlex.rbj \
  $(LIBS)

$(BIN)/fsashortlex.exe: $(FSASHORTLEX_EXE) 
	$(LINK32) $(FSASHORTLEX_EXE) $(L32EXE) -out:$@ -map:$M 

FSASTAR_EXE = \
  fsastar.$O \
  fsastar.rbj \
  $(LIBS)

$(BIN)/fsastar.exe: $(FSASTAR_EXE) 
	$(LINK32) $(FSASTAR_EXE) $(L32EXE) -out:$@ -map:$M 

FSANOT_EXE = \
  fsanot.$O \
  fsanot.rbj \
  $(LIBS)

$(BIN)/fsanot.exe: $(FSANOT_EXE) 
	$(LINK32) $(FSANOT_EXE) $(L32EXE) -out:$@ -map:$M 

FSAREVERSE_EXE = \
  fsareverse.$O \
  fsareverse.rbj \
  $(LIBS)

$(BIN)/fsareverse.exe: $(FSAREVERSE_EXE) 
	$(LINK32) $(FSAREVERSE_EXE) $(L32EXE) -out:$@ -map:$M 

FSASEPARATE_EXE = \
  fsaseparate.$O \
  fsaseparate.rbj \
  $(LIBS)

$(BIN)/fsaseparate.exe: $(FSASEPARATE_EXE) 
	$(LINK32) $(FSASEPARATE_EXE) $(L32EXE) -out:$@ -map:$M 

FSASWAPCOORDS_EXE = \
  fsaswapcoords.$O \
  fsaswapcoords.rbj \
  $(LIBS)

$(BIN)/fsaswapcoords.exe: $(FSASWAPCOORDS_EXE) 
	$(LINK32) $(FSASWAPCOORDS_EXE) $(L32EXE) -out:$@ -map:$M 

MIDFADETERMINIZE_EXE = \
  midfadeterminize.$O \
  midfadeterminize.rbj \
  $(LIBS)

$(BIN)/midfadeterminize.exe: $(MIDFADETERMINIZE_EXE)
	$(LINK32) $(MIDFADETERMINIZE_EXE) $(L32EXE) -out:$@ -map:$M 

RWSPRINT_EXE = \
  rwsprint.$O \
  rwsprint.rbj \
  $(LIBS)

$(BIN)/rwsprint.exe: $(RWSPRINT_EXE) 
	$(LINK32) $(RWSPRINT_EXE) $(L32EXE) -out:$@ -map:$M 

MAKECOSFILE_EXE = \
  makecos.$O \
  makecosfile.rbj \
  $(LIBS)

$(BIN)/makecosfile.exe: $(MAKECOSFILE_EXE) 
	$(LINK32) $(MAKECOSFILE_EXE) $(L32EXE) -out:$@ -map:$M 

GPAXIOMS_EXE = \
  gpaxioms.$O \
  gpaxioms.rbj \
  $(LIBS)

$(BIN)/gpaxioms.exe: $(GPAXIOMS_EXE) 
	$(LINK32) $(GPAXIOMS_EXE) $(L32EXE) -out:$@ -map:$M 

GPCCLASS_EXE = \
  gpcclass.$O \
  gpcclass.rbj \
  $(LIBS)

$(BIN)/gpcclass.exe: $(GPCCLASS_EXE) 
	$(LINK32) $(GPCCLASS_EXE) $(L32EXE) -out:$@ -map:$M 

GPCOSETS_EXE = \
  gpcosets.$O \
  gpcosets.rbj \
  $(LIBS)

$(BIN)/gpcosets.exe: $(GPCOSETS_EXE) 
	$(LINK32) $(GPCOSETS_EXE) $(L32EXE) -out:$@ -map:$M 

GPMULT_EXE = \
  gpmult.$O \
  gpmult.rbj \
  $(LIBS)

$(BIN)/gpmult.exe: $(GPMULT_EXE) 
	$(LINK32) $(GPMULT_EXE) $(L32EXE) -out:$@ -map:$M 

GPMULT2_EXE = \
  gpmult2.$O \
  gpmult2.rbj \
  $(LIBS)

$(BIN)/gpmult2.exe: $(GPMULT2_EXE) 
	$(LINK32) $(GPMULT2_EXE) $(L32EXE) -out:$@ -map:$M 

GPMIMULT_EXE = \
  gpmimult.$O \
  gpmimult.rbj \
  $(LIBS)

$(BIN)/gpmimult.exe: $(GPMIMULT_EXE) 
	$(LINK32) $(GPMIMULT_EXE) $(L32EXE) -out:$@ -map:$M 

GPMIMULT2_EXE = \
  gpmimult2.$O \
  gpmimult2.rbj \
  $(LIBS)

$(BIN)/gpmimult2.exe: $(GPMIMULT2_EXE) 
	$(LINK32) $(GPMIMULT2_EXE) $(L32EXE) -out:$@ -map:$M 

GPMINKB_EXE = \
  gpminkb.$O \
  gpminkb.rbj \
  $(LIBS)

$(BIN)/gpminkb.exe: $(GPMINKB_EXE) 
	$(LINK32) $(GPMINKB_EXE) $(L32EXE) -out:$@ -map:$M 

GPMIGMDET_EXE = \
  gpmigmdet.$O \
  gpmigmdet.rbj \
  $(LIBS)

$(BIN)/gpmigmdet.exe: $(GPMIGMDET_EXE) 
	$(LINK32) $(GPMIGMDET_EXE) $(L32EXE) -out:$@ -map:$M 

GPMAKEFSA_EXE = \
  gpmakefsa.$O \
  gpmakefsa.rbj \
  $(LIBS)

$(BIN)/gpmakefsa.exe: $(GPMAKEFSA_EXE) 
	$(LINK32) $(GPMAKEFSA_EXE) $(L32EXE) -out:$@ -map:$M 

GPGENMULT2_EXE = \
  gpgenmult2.$O \
  gpgenmult2.rbj \
  $(LIBS)

$(BIN)/gpgenmult2.exe: $(GPGENMULT2_EXE) 
	$(LINK32) $(GPGENMULT2_EXE) $(L32EXE) -out:$@ -map:$M 

GPMIGENMULT2_EXE = \
  gpmigenmult2.$O \
  gpmigenmult2.rbj \
  $(LIBS)

$(BIN)/gpmigenmult2.exe: $(GPMIGENMULT2_EXE) 
	$(LINK32) $(GPMIGENMULT2_EXE) $(L32EXE) -out:$@ -map:$M 

GPGEOWA_EXE = \
  gpgeowa.$O \
  gpgeowa.rbj \
  $(LIBS)

$(BIN)/gpgeowa.exe: $(GPGEOWA_EXE) 
	$(LINK32) $(GPGEOWA_EXE) $(L32EXE) -out:$@ -map:$M 

GPOVLWA_EXE = \
  gpovlwa.$O \
  gpovlwa.rbj \
  $(LIBS)

$(BIN)/gpovlwa.exe: $(GPOVLWA_EXE) 
	$(LINK32) $(GPOVLWA_EXE) $(L32EXE) -out:$@ -map:$M 

GPSTABILISER_EXE = \
  gpstabiliser.$O \
  gpstabiliser.rbj \
  $(LIBS)

$(BIN)/gpstabiliser.exe: $(GPSTABILISER_EXE) 
	$(LINK32) $(GPSTABILISER_EXE) $(L32EXE) -out:$@ -map:$M 


GPSUBLOWINDEX_EXE = \
  gpsublowindex.$O \
  gpsublowindex.rbj \
  $(LIBS)

$(BIN)/gpsublowindex.exe: $(GPSUBLOWINDEX_EXE) 
	$(LINK32) $(GPSUBLOWINDEX_EXE) $(L32EXE) -out:$@ -map:$M 

GPSUBMAKE_EXE = \
  gpsubmake.$O \
  gpsubmake.rbj \
  $(LIBS)

$(BIN)/gpsubmake.exe: $(GPSUBMAKE_EXE) 
	$(LINK32) $(GPSUBMAKE_EXE) $(L32EXE) -out:$@ -map:$M 

GPSUBPRES_EXE = \
  gpsubpres.$O \
  gpsubpres.rbj \
  $(LIBS)

$(BIN)/gpsubpres.exe: $(GPSUBPRES_EXE) 
	$(LINK32) $(GPSUBPRES_EXE) $(L32EXE) -out:$@ -map:$M 

GPSUBWA_EXE = \
  gpsubwa.$O \
  gpsubwa.rbj \
  $(LIBS)

$(BIN)/gpsubwa.exe: $(GPSUBWA_EXE) 
	$(LINK32) $(GPSUBWA_EXE) $(L32EXE) -out:$@ -map:$M 

GPTCENUM_EXE = \
  gptcenum.$O \
  gptcenum.rbj \
  $(LIBS)

$(BIN)/gptcenum.exe: $(GPTCENUM_EXE) 
	$(LINK32) $(GPTCENUM_EXE) $(L32EXE) -out:$@ -map:$M 

GPVITAL_EXE = \
  gpvital.$O \
  gpvital.rbj \
  $(LIBS)

$(BIN)/gpvital.exe: $(GPVITAL_EXE) 
	$(LINK32) $(GPVITAL_EXE) $(L32EXE) -out:$@ -map:$M 

GPWA_EXE = \
  gpwa.$O \
  gpwa.rbj \
  $(LIBS)

$(BIN)/gpwa.exe: $(GPWA_EXE) 
	$(LINK32) $(GPWA_EXE) $(L32EXE) -out:$@ -map:$M 

GPXLATWA_EXE = \
  gpxlatwa.$O \
  gpxlatwa.rbj \
  $(LIBS)

$(BIN)/gpxlatwa.exe: $(GPXLATWA_EXE) 
	$(LINK32) $(GPXLATWA_EXE) $(L32EXE) -out:$@ -map:$M 

GPDIFFLABS_EXE = \
  gpdifflabs.$O \
  gpdifflabs.rbj \
  $(LIBS)

$(BIN)/gpdifflabs.exe: $(GPDIFFLABS_EXE) 
	$(LINK32) $(GPDIFFLABS_EXE) $(L32EXE) -out:$@ -map:$M 

GPMORPHISM_EXE = \
  gpmorphism.$O \
  gpmorphism.rbj \
  $(LIBS)

$(BIN)/gpmorphism.exe: $(GPMORPHISM_EXE) 
	$(LINK32) $(GPMORPHISM_EXE) $(L32EXE) -out:$@ -map:$M 

FSALEQUAL_EXE = \
  fsalequal.$O \
  fsalequal.rbj \
  $(LIBS)

$(BIN)/fsalequal.exe: $(FSALEQUAL_EXE) 
	$(LINK32) $(FSALEQUAL_EXE) $(L32EXE) -out:$@ -map:$M 

FSACARTESIAN_EXE = \
  fsacartesian.$O \
  fsacartesian.rbj \
  $(LIBS)

$(BIN)/fsacartesian.exe: $(FSACARTESIAN_EXE) 
	$(LINK32) $(FSACARTESIAN_EXE) $(L32EXE) -out:$@ -map:$M 

FSACOMPOSE_EXE = \
  fsacompose.$O \
  fsacompose.rbj \
  $(LIBS)

$(BIN)/fsacompose.exe: $(FSACOMPOSE_EXE) 
	$(LINK32) $(FSACOMPOSE_EXE) $(L32EXE) -out:$@ -map:$M 

ISNORMAL_EXE = \
  isnormal.$O \
  isnormal.rbj \
  $(LIBS)

$(BIN)/isnormal.exe: $(ISNORMAL_EXE) 
	$(LINK32) $(ISNORMAL_EXE) $(L32EXE) -out:$@ -map:$M 

ISCONJUGATE_EXE = \
  isconjugate.$O \
  isconjugate.rbj \
  $(LIBS)

$(BIN)/isconjugate.exe: $(ISCONJUGATE_EXE) 
	$(LINK32) $(ISCONJUGATE_EXE) $(L32EXE) -out:$@ -map:$M 

SIMPLIFY_EXE = \
  simplify.$O \
  simplify.rbj \
  $(LIBS)

$(BIN)/simplify.exe: $(SIMPLIFY_EXE) 
	$(LINK32) $(SIMPLIFY_EXE) $(L32EXE) -out:$@ -map:$M 

STARTUP = \
  CRT0TCON.$W \
  CRT0TWIN.$W \
  DLLCRT0.$W \
  exit.$W

CRTLIB = \
  initterm.$W \
  argcargv.$W \
  atoi.$O \
  calloc.$O  \
  clock.$W \
  malloc.$W \
  printfw.$W \
  purevirt.$O \
  signal.$W \
  sprintf.$O \
  strchr.$O \
  strcmp.$O \
  time.$W \
  x_to_str.$O

startup.lib : $(STARTUP)
	$(LIB32) -out:startup.lib $(STARTUP)

MAF_DLL = \
  dllcrt0.$W \
  mafdll.$W \
  exit.$W \
  $(MAFLIB) \
  $(CRTLIB) \
  maf.rbj

$(BIN)/maf.dll: $(MAF_DLL) maf.def
	$(LINK32) $(MAF_DLL) $(L32DLL) -def:maf.def -out:maf.dll -map:$M
	-mkdir "$(BIN)"
	move maf.dll "$@"

maf.lib : $(BIN)/maf.dll

include maf.dep

#extra dependencies to get the Unix files checked in to the rcs archive as well
#unmake annoyingly won't list the dependencies unless there is a real command.
dummy: mafu.mak mafu.dep mafu64.mak
	@echo Build complete
