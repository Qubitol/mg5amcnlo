      Subroutine prepare_grouping_choice(PP, WGT, INIT)
C     ****************************************************
C     
C     Generated by MadGraph5_aMC@NLO v. 3.1.0, 2021-03-30
C     By the MadGraph5_aMC@NLO Development Team
C     Visit launchpad.net/madgraph5 and amcatnlo.web.cern.ch
C      
C     INPUT
C       PP : MOMENTA
C       INIT: FLAG to RESET CUMULATIVE VARIABLE
C             (set on True for event by event selection)
C     WGT: Jacobian used so far (no update here)
C
C     OUTPUT:
C        SELPROC()
C        SUMPROB
C     ****************************************************      
      USE DISCRETESAMPLER
      IMPLICIT NONE


      
      INCLUDE 'genps.inc'
      INCLUDE 'maxconfigs.inc'
      INCLUDE 'nexternal.inc'
      INCLUDE 'maxamps.inc'
      
      INTEGER I,J, IPROC, IMIRROR
      DOUBLE PRECISION PP(*), WGT
      LOGICAL INIT
      
      
      DOUBLE PRECISION SELPROC(2, MAXSPROC, LMAXCONFIGS)
      INTEGER LARGEDIM
      PARAMETER (LARGEDIM=2*MAXSPROC*LMAXCONFIGS)
      DATA SELPROC/LARGEDIM*0D0/
      DOUBLE PRECISION SUMPROB
      DATA SUMPROB/0D0/
      common /to_grouping_selection/sumprob,selproc
      
c     TODO: MOVE THIS AS A COMMON BLOCK?      
      INTEGER CONFSUB(MAXSPROC,LMAXCONFIGS)
      INCLUDE 'config_subproc_map.inc'
      INTEGER PERMS(NEXTERNAL,LMAXCONFIGS)
      INCLUDE 'symperms.inc'
      LOGICAL MIRRORPROCS(MAXSPROC)
      INCLUDE 'mirrorprocs.inc'      

      INTEGER SYMCONF(0:LMAXCONFIGS)
      COMMON /TO_SYMCONF/ SYMCONF


      DOUBLE PRECISION XDUM, XSDUM, DUM

      INTEGER LMAPPED

      Double Precision DSIGPROC
      include 'vector.inc'
      include 'run.inc'
C     To limit the number of calls to switchmom, use in DSIGPROC the
C      cached variable last_iconfig. It is in this subroutine as well
C      so that we can set it to -1 to ignore caching (to prevent
C      undesired effect if this subroutine is called from elsewhere
C      and to 0 to reset the cache.
      INTEGER LAST_ICONF
      DATA LAST_ICONF/-1/
      COMMON/TO_LAST_ICONF/LAST_ICONF
      
      LOGICAL INIT_MODE
      COMMON /TO_DETERMINE_ZERO_HEL/INIT_MODE      
C     CM_RAP has parton-parton system rapidity -> need to check if track correctly
      DOUBLE PRECISION CM_RAP
      LOGICAL SET_CM_RAP
      COMMON/TO_CM_RAP/SET_CM_RAP,CM_RAP
      
C     Select among the subprocesses based on PDF weight
      if(init)then
         SUMPROB=0D0
         SELPROC(:,:,:) = 0d0         
      endif
C     Turn caching on in dsigproc to avoid too many calls to switchmom
      LAST_ICONF=0
      DO J=1,SYMCONF(0)
        DO IPROC=1,MAXSPROC
          IF(INIT_MODE.OR.CONFSUB(IPROC,SYMCONF(J)).NE.0) THEN
            DO IMIRROR=1,2
              IF(IMIRROR.EQ.1.OR.MIRRORPROCS(IPROC))THEN
C               Calculate PDF weight for all subprocesses
		XSDUM =  DSIGPROC(PP,J,IPROC,IMIRROR,SYMCONF,CONFSUB,DUM,4)
                 SELPROC(IMIRROR,IPROC,J)= SELPROC(IMIRROR,IPROC,J) + XSDUM
                IF(MC_GROUPED_SUBPROC) THEN
                  CALL MAP_3_TO_1(J,IPROC,IMIRROR,MAXSPROC,2,LMAPPED)
                  CALL DS_ADD_ENTRY('PDF_convolution',LMAPPED
     $             , XSDUM,.TRUE.)
                ENDIF
                SUMPROB=SUMPROB+XSDUM
                IF(IMIRROR.EQ.2)THEN
C                 Need to flip back x values
                  XDUM=XBK(1)
                  XBK(1)=XBK(2)
                  XBK(2)=XDUM
                  CM_RAP=-CM_RAP
                ENDIF
              ENDIF
            ENDDO
          ENDIF
        ENDDO
      ENDDO
C     Turn caching in dsigproc back off to avoid side effects.
      LAST_ICONF=-1

C     Cannot make a selection with all PDFs to zero, so we return now
      IF(SUMPROB.EQ.0.0D0) THEN
        RETURN
      ENDIF
      END
      
      SUBROUTINE select_grouping(imirror, iproc, iconf, wgt, IWARP)
      USE DISCRETESAMPLER
      IMPLICIT NONE
c     
c     INPUT (VIA COMMAND BLOCK)
c       SELPROC 
c       SUMPROB
c     INPUT
c       VECSIZE_USED (number of weight to update)
c     INPUT/OUTPUT
c       WGT(VECSIZE_USED) #multiplied by the associated jacobian      
c
c     OUTPUT
c
c     iconf, iproc, imirror
c
      integer  IWARP
      integer IVEC
      double precision WGT(*)
      integer imirror, iproc, iconf

C     
C     CONSTANTS
C     
      INCLUDE 'genps.inc'
      INCLUDE 'maxconfigs.inc'
      INCLUDE 'nexternal.inc'
      INCLUDE 'maxamps.inc'
c     
      double precision R
c
      DOUBLE PRECISION SELPROC(2, MAXSPROC, LMAXCONFIGS)
      INTEGER LARGEDIM
      PARAMETER (LARGEDIM=2*MAXSPROC*LMAXCONFIGS)
      double precision sumprob
      common /to_grouping_selection/sumprob,selproc      

      INTEGER SYMCONF(0:LMAXCONFIGS)
      COMMON /TO_SYMCONF/ SYMCONF
c
c     LOCAL
c      
      integer i,J,K
      double precision TOTWGT
      INTEGER CONFSUB(MAXSPROC,LMAXCONFIGS)
      include 'config_subproc_map.inc'
      
c      
c     VARIABLE FOR THE MC over proccess with importance sampling additional factor
c
      logical init_mode
      common/to_determine_zero_hel/init_mode
      INTEGER GROUPED_MC_GRID_STATUS
      REAL*8 MC_GROUPED_PROC_JACOBIAN
      INTEGER LMAPPED
      include 'vector.inc'
      include 'run.inc'
C     Perform the selection
      CALL RANMAR(R)

C     It is important to cache the status before adding any entries to
C      this grid in this
C     routine since it might change it
      GROUPED_MC_GRID_STATUS = DS_GET_DIM_STATUS('grouped_processes')

      
C     If we are still initializing the grid or simply not using one at
C      all, then we pick a point based on PDF only.
      IF (.NOT.MC_GROUPED_SUBPROC.OR.GROUPED_MC_GRID_STATUS.EQ.0) THEN
        R=R*SUMPROB
        ICONF=0
        IPROC=0
        TOTWGT=0D0
        DO J=1,SYMCONF(0)
          DO I=1,MAXSPROC
            IF(INIT_MODE.OR.CONFSUB(I,SYMCONF(J)).NE.0) THEN
              DO K=1,2
                TOTWGT=TOTWGT+SELPROC(K,I,J)
                IF(R.LT.TOTWGT)THEN
                  IPROC=I
                  ICONF=J
                  IMIRROR=K
                  GOTO 50
                ENDIF
              ENDDO
            ENDIF
          ENDDO
        ENDDO
 50     CONTINUE
C       Update weigth w.r.t SELPROC normalized to selection probability

	DO I=1, WARP_SIZE
	   IVEC = (IWARP -1) *WARP_SIZE + I
	   WGT(IVEC)=WGT(IVEC)*(SUMPROB/SELPROC(IMIRROR,IPROC,ICONF))
	ENDDO	

      ELSE
C       We are using the grouped_processes grid and it is initialized.
        CALL DS_GET_POINT('grouped_processes',R,LMAPPED
     $   ,MC_GROUPED_PROC_JACOBIAN,'norm',(/'PDF_convolution'/))
     	DO I=1, WARP_SIZE
          IVEC = (IWARP -1) *WARP_SIZE + I
	  WGT(IVEC)=WGT(IVEC)*MC_GROUPED_PROC_JACOBIAN
	ENDDO	
        CALL MAP_1_TO_3(LMAPPED,MAXSPROC,2,ICONF,IPROC,IMIRROR)
      ENDIF
      RETURN
      END

      SUBROUTINE DSIG_VEC(ALL_P,ALL_WGT,all_xbk,all_q2fact,all_cm_rap,ICONF_vec,IPROC,IMIRROR_vec,ALL_OUT,VECSIZE_USED)
c     ******************************************************
c
c     INPUT: ALL_PP(0:3, NEXTERNAL, VECSIZE_USED)
c     INPUT/OUtpUT       ALL_WGT(VECSIZE_USED)
c            VECSIZE_USED = vector size
c     ALL_OUT(VECSIZE_USED)
c            function (PDf*cross)
c     ******************************************************
      USE DISCRETESAMPLER
      IMPLICIT NONE

      integer VECSIZE_USED
      include 'vector.inc'
      include 'genps.inc'
      double precision all_p(4*maxdim/3+14,*)
      double precision all_wgt(*)
      double precision all_xbk(2,*)
      double precision all_q2fact(2,*)
      double precision all_cm_rap(*)
      integer iconf_vec(NB_WARP), iproc, imirror_vec(NB_WARP)
      double precision all_out(*)
      include 'maxconfigs.inc'
      include 'maxamps.inc'

      INTEGER LARGEDIM
      PARAMETER (LARGEDIM=2*MAXSPROC*LMAXCONFIGS)

      INTEGER CONFSUB(MAXSPROC,LMAXCONFIGS)
      INCLUDE 'config_subproc_map.inc'

C     SUBDIAG is vector of diagram numbers for this config
C     IB gives which beam is which (for mirror processes)
      INTEGER SUBDIAG(MAXSPROC),IB(2)
      COMMON/TO_SUB_DIAG/SUBDIAG,IB

      INTEGER MAPCONFIG(0:LMAXCONFIGS), ICONFIG
      COMMON/TO_MCONFIGS/MAPCONFIG, ICONFIG

      DOUBLE PRECISION SUMWGT(2, MAXSPROC,LMAXCONFIGS)
      INTEGER NUMEVTS(2, MAXSPROC,LMAXCONFIGS)
      COMMON /DSIG_SUMPROC/SUMWGT,NUMEVTS

      DOUBLE PRECISION DSIGPROC

      INTEGER SYMCONF(0:LMAXCONFIGS)
      COMMON /TO_SYMCONF/ SYMCONF

      INTEGER IMIRROR_GLOBAL, IPROC_GLOBAL
      COMMON/TO_MIRROR/ IMIRROR_GLOBAL, IPROC_GLOBAL

      DOUBLE PRECISION SELPROC(2, MAXSPROC, LMAXCONFIGS)
      double precision sumprob
      common /to_grouping_selection/sumprob,selproc

      LOGICAL CUTSDONE,CUTSPASSED
      COMMON/TO_CUTSDONE/CUTSDONE,CUTSPASSED      

      INTEGER I, CURR_WARP, NB_WARP_USED
      integer grouped_MC_grid_status

      integer                                      lpp(2)
      double precision    ebeam(2), xbk(2),q2fact(2)
      common/to_collider/ ebeam   , xbk   ,q2fact,   lpp

      DOUBLE PRECISION CM_RAP
      LOGICAL SET_CM_RAP
      COMMON/TO_CM_RAP/SET_CM_RAP,CM_RAP

C     To be able to control when the matrix<i> subroutine can add entries to the grid for the MC over helicity configuration
      LOGICAL ALLOW_HELICITY_GRID_ENTRIES
	  DATA ALLOW_HELICITY_GRID_ENTRIES/.True./
	  COMMON/TO_ALLOW_HELICITY_GRID_ENTRIES/ALLOW_HELICITY_GRID_ENTRIES


       grouped_MC_grid_status = DS_get_dim_status('grouped_processes')
       IMIRROR_GLOBAL = IMIRROR_VEC(1)
       IPROC_GLOBAL = IPROC
c       ICONFIG=SYMCONF(ICONF_VEC(1))
c       DO I=1,MAXSPROC
c       	    SUBDIAG(I) = CONFSUB(I,SYMCONF(ICONF_VEC(1)))
c      	ENDDO

c     set the running scale 
c     and update the couplings accordingly
      call update_scale_coupling_vec(all_p, all_wgt, all_q2fact, VECSIZE_USED)

      IF(grouped_MC_grid_status.eq.0) then
C       If we were in the initialization phase of the grid for MC over grouped processes, we must instruct the matrix<i> subroutine not to add again an entry in the grid for this PS point at the call DSIGPROC just below.
      	  ALLOW_HELICITY_GRID_ENTRIES = .False.
      endif

      call DSIGPROC_vec(ALL_P,ALL_XBK,ALL_Q2FACT,ALL_CM_RAP,ICONF_VEC,IPROC,IMIRROR_VEC,SYMCONF,CONFSUB,ALL_WGT,0,ALL_OUT,VECSIZE_USED)

      
      Do i =1,VECSIZE_USED
c     Reset ALLOW_HELICITY_GRID_ENTRIES
	  ALLOW_HELICITY_GRID_ENTRIES = .True. 

c     Below is how one would go about adding each point to the grouped_processes grid
C     However, keeping only the initialization grid is better because in that case all grouped ME's
C     were computed with the same kinematics. For this reason, the code below remains commented.
c	  IF(grouped_MC_grid_status.ge.1) then
c	    call map_3_to_1(ICONF,IPROC,IMIRROR,MAXSPROC,2,Lmapped)
c		call DS_add_entry('grouped_processes',Lmapped,(ALL_OUT(i)/SELPROC(IMIRROR,IPROC,ICONF)))
c	  ENDIF

	  ENDDO

      NB_WARP_USED = VECSIZE_USED / WARP_SIZE
      IF( NB_WARP_USED * WARP_SIZE .NE. VECSIZE_USED ) THEN
        WRITE(*,*) 'ERROR: NB_WARP_USED * WARP_SIZE .NE. VECSIZE_USED',
     &    NB_WARP_USED, WARP_SIZE, VECSIZE_USED
        STOP
      ENDIF

      DO CURR_WARP=1, NB_WARP_USED
         DO I=(CURR_WARP-1)*WARP_SIZE+1,CURR_WARP*WARP_SIZE
	    IF(ALL_OUT(I).GT.0d0)THEN
C     Update summed weight and number of events
             SUMWGT(IMIRROR_VEC(CURR_WARP),IPROC,ICONF_VEC(CURR_WARP))=SUMWGT(IMIRROR_VEC(CURR_WARP),IPROC,ICONF_VEC(CURR_WARP))+DABS(ALL_OUT(I)*ALL_WGT(I))
             NUMEVTS(IMIRROR_VEC(CURR_WARP),IPROC,ICONF_VEC(CURR_WARP))=NUMEVTS(IMIRROR_VEC(CURR_WARP),IPROC,ICONF_VEC(CURR_WARP))+1
      	    ENDIF  
         ENDDO
      ENDDO

      RETURN
      END

DOUBLE PRECISION FUNCTION DSIG(PP,WGT,IMODE)
C ****************************************************
C
%(info_lines)s
C
%(process_lines)s
C
C     RETURNS DIFFERENTIAL CROSS SECTION 
c     FOR MULTIPLE PROCESSES IN PROCESS GROUP
C     Input:
C             pp    4 momentum of external particles
C             wgt   weight from Monte Carlo
C             imode 0 run, 1 init, 2 reweight,
C                   3 finalize, 4 only PDFs
C     Output:
C             Amplitude squared and summed
C ****************************************************
      use DiscreteSampler
      IMPLICIT NONE
C  
C CONSTANTS
C  
      include 'genps.inc'
      include 'maxconfigs.inc'
      include 'nexternal.inc'
      include 'maxamps.inc'
      REAL*8     PI
      PARAMETER (PI=3.1415926d0)
C  
C ARGUMENTS 
C  
      DOUBLE PRECISION PP(0:3,NEXTERNAL), WGT
      INTEGER IMODE
C   
C LOCAL VARIABLES 
C  
      integer Lmapped
      INTEGER I,J,K,LUN,ICONF,IMIRROR,NPROC
      SAVE NPROC
      INTEGER SYMCONF(0:LMAXCONFIGS)
      COMMON /TO_SYMCONF/ SYMCONF
      DOUBLE PRECISION SUMPROB,TOTWGT,R,XDUM
      INTEGER CONFSUB(MAXSPROC,LMAXCONFIGS)
      INCLUDE 'config_subproc_map.inc'            
      INTEGER PERMS(NEXTERNAL,LMAXCONFIGS)
      INCLUDE 'symperms.inc'
      LOGICAL MIRRORPROCS(MAXSPROC)
      INCLUDE 'mirrorprocs.inc'
C   SELPROC is vector of selection weights for the subprocesses
C   SUMWGT is vector of total weight for the subprocesses
C   NUMEVTS is vector of event calls for the subprocesses
      DOUBLE PRECISION SELPROC(2, MAXSPROC,LMAXCONFIGS)
      DOUBLE PRECISION SUMWGT(2, MAXSPROC,LMAXCONFIGS)
      INTEGER NUMEVTS(2, MAXSPROC,LMAXCONFIGS)
      INTEGER LARGEDIM
      PARAMETER (LARGEDIM=2*MAXSPROC*LMAXCONFIGS)
      DATA SELPROC/LARGEDIM*0d0/
      DATA SUMWGT/LARGEDIM*0d0/
      DATA NUMEVTS/LARGEDIM*0/
      SAVE SELPROC
      COMMON /DSIG_SUMPROC/SUMWGT,NUMEVTS
	  real*8 MC_grouped_proc_jacobian
      integer grouped_MC_grid_status	  
C  
C EXTERNAL FUNCTIONS
C  
      INTEGER NEXTUNOPEN
      DOUBLE PRECISION DSIGPROC
      EXTERNAL NEXTUNOPEN,DSIGPROC
C  
C GLOBAL VARIABLES
C  
C     Common blocks

      include '../../Source/PDF/pdf.inc'
C      CHARACTER*7         PDLABEL,EPA_LABEL
C      INTEGER       LHAID
C      COMMON/TO_PDF/LHAID,PDLABEL,EPA_LABEL

      integer nb_spin_state(2)
      data  nb_spin_state /%(nb_spin_state1)i,%(nb_spin_state2)i/
      common /nb_hel_state/ nb_spin_state

      include 'vector.inc' ! defines VECSIZE_MEMMAX
      include 'coupl.inc' ! needs VECSIZE_MEMMAX (defined in vector.inc)
      include 'run.inc'
C   ICONFIG has this config number
      INTEGER MAPCONFIG(0:LMAXCONFIGS), ICONFIG
      COMMON/TO_MCONFIGS/MAPCONFIG, ICONFIG
C     IPROC has the present process number
      INTEGER IPROC
      COMMON/TO_MIRROR/IMIRROR, IPROC
C     CM_RAP has parton-parton system rapidity
      DOUBLE PRECISION CM_RAP
      LOGICAL SET_CM_RAP
      COMMON/TO_CM_RAP/SET_CM_RAP,CM_RAP
C     Keep track of whether cuts already calculated for this event
      LOGICAL CUTSDONE,CUTSPASSED
      COMMON/TO_CUTSDONE/CUTSDONE,CUTSPASSED
C     To be able to control when the matrix<i> subroutine can add entries to the grid for the MC over helicity configuration
      LOGICAL ALLOW_HELICITY_GRID_ENTRIES
	  DATA ALLOW_HELICITY_GRID_ENTRIES/.True./
	  COMMON/TO_ALLOW_HELICITY_GRID_ENTRIES/ALLOW_HELICITY_GRID_ENTRIES
c     To limit the number of calls to switchmom, use in DSIGPROC the cached variable last_iconfig. It is in this subroutine as well so that we can set it to -1 to ignore caching (to prevent undesired effect if this subroutine is called from elsewhere and to 0 to reset the cache.
	  INTEGER LAST_ICONF
	  DATA LAST_ICONF/-1/
	  COMMON/TO_LAST_ICONF/LAST_ICONF

	  double precision DUM
      logical init_mode
      common /to_determine_zero_hel/init_mode
C ----------
C BEGIN CODE
C ----------
      DSIG=0D0

C     Make sure cuts are evaluated for first subprocess
C      CUTSDONE=.FALSE.
C      CUTSPASSED=.FALSE.

	  if(pdlabel.eq.'dressed'.and.DS_get_dim_status('ee_mc').eq.-1)then
	  	  call DS_register_dimension('ee_mc', 0)
c		  ! set both mode 1: resonances, 2: no resonances to 50-50
		  call DS_add_bin('ee_mc', 1)
		  call DS_add_bin('ee_mc', 2)
		  call DS_add_entry('ee_mc', 1, 0.5d0, .true.)
		  call DS_add_entry('ee_mc', 2, 0.5d0, .true.)
		  call DS_update_grid('ee_mc')
          endif				      



      IF(IMODE.EQ.1)THEN
C       Set up process information from file symfact
        LUN=NEXTUNOPEN()
        IPROC=1
        SYMCONF(IPROC)=ICONFIG
        OPEN(UNIT=LUN,FILE='../symfact.dat',STATUS='OLD',ERR=20)
        DO WHILE(.TRUE.)
          READ(LUN,*,ERR=10,END=10) XDUM, ICONF
          IF(ICONF.EQ.-MAPCONFIG(ICONFIG))THEN
            IPROC=IPROC+1
            SYMCONF(IPROC)=INT(XDUM)
          ENDIF
        ENDDO
 10     SYMCONF(0)=IPROC
        CLOSE(LUN)
        RETURN
 20     SYMCONF(0)=IPROC
        WRITE(*,*)'Error opening symfact.dat. No permutations used.'
        RETURN
      ELSE IF(IMODE.EQ.2)THEN
C     Output weights and number of events
        SUMPROB=0D0
        DO J=1,SYMCONF(0)
          DO I=1,MAXSPROC
            DO K=1,2
              SUMPROB=SUMPROB+SUMWGT(K,I,J)
            ENDDO
          ENDDO
        ENDDO
        WRITE(*,*)'Relative summed weights:'
	if (SUMPROB.ne.0d0)then
        DO J=1,SYMCONF(0)
          WRITE(*,'(%(nsprocs)dE12.4)')((SUMWGT(K,I,J)/SUMPROB,K=1,2),I=1,MAXSPROC)
        ENDDO
	endif
        SUMPROB=0D0
        DO J=1,SYMCONF(0)
          DO I=1,MAXSPROC
            DO K=1,2
              SUMPROB=SUMPROB+NUMEVTS(K,I,J)
            ENDDO
          ENDDO
        ENDDO
        WRITE(*,*)'Relative number of events:'
	if (SUMPROB.ne.0d0)then
        DO J=1,SYMCONF(0)
          WRITE(*,'(%(nsprocs)dE12.4)')((NUMEVTS(K,I,J)/SUMPROB,K=1,2),I=1,MAXSPROC)
        ENDDO
	ENDIF
        WRITE(*,*)'Events:'
        DO J=1,SYMCONF(0)
          WRITE(*,'(%(nsprocs)dI12)')((NUMEVTS(K,I,J),K=1,2),I=1,MAXSPROC)
        ENDDO
C       Reset weights and number of events
        DO J=1,SYMCONF(0)
          DO I=1,MAXSPROC
            DO K=1,2
              NUMEVTS(K,I,J)=0
              SUMWGT(K,I,J)=0d0
            ENDDO
          ENDDO
        ENDDO
        RETURN
      ELSE IF(IMODE.EQ.3)THEN
C     No finalize needed
        RETURN
      ENDIF

C   IMODE.EQ.0, regular run mode
	  IF(MC_grouped_subproc.and.DS_get_dim_status('grouped_processes').eq.-1) then
	    call DS_register_dimension('grouped_processes', 0)
		call DS_set_min_points(10,'grouped_processes')
        DO J=1,SYMCONF(0)
          DO IPROC=1,MAXSPROC
            IF(init_mode.or.CONFSUB(IPROC,SYMCONF(J)).NE.0) THEN
              DO IMIRROR=1,2
                IF(IMIRROR.EQ.1.OR.MIRRORPROCS(IPROC))THEN
                  CALL MAP_3_TO_1(J,IPROC,IMIRROR,MAXSPROC,2,LMAPPED)
                  CALL DS_add_bin('grouped_processes',LMAPPED)
                ENDIF
              ENDDO
            ENDIF
          ENDDO
        ENDDO
	  ENDIF
	  IF(MC_grouped_subproc.and.DS_dim_index(run_grid, 'PDF_convolution',.True.).eq.-1) then
	    call DS_register_dimension('PDF_convolution', 0, all_grids=.False.)
	  ENDIF


C     Select among the subprocesses based on PDF weight
      SUMPROB=0D0
C     Turn caching on in dsigproc to avoid too many calls to switchmom
      LAST_ICONF=0
      DO J=1,SYMCONF(0)
         DO IPROC=1,MAXSPROC
            IF(init_mode.or.CONFSUB(IPROC,SYMCONF(J)).NE.0) THEN
               DO IMIRROR=1,2
                  IF(IMIRROR.EQ.1.OR.MIRRORPROCS(IPROC))THEN
C                 Calculate PDF weight for all subprocesses
                     SELPROC(IMIRROR,IPROC,J)=DSIGPROC(PP,J,IPROC,IMIRROR,SYMCONF,CONFSUB,DUM,4)
                     IF(MC_grouped_subproc) then
					   call map_3_to_1(J,IPROC,IMIRROR,MAXSPROC,2,Lmapped)
					   call DS_add_entry('PDF_convolution',Lmapped,SELPROC(IMIRROR,IPROC,J),.True.)
					 endif
                     SUMPROB=SUMPROB+SELPROC(IMIRROR,IPROC,J)
                     IF(IMIRROR.EQ.2)THEN
C                 Need to flip back x values
                        XDUM=XBK(1)
                        XBK(1)=XBK(2)
                        XBK(2)=XDUM
                        CM_RAP=-CM_RAP
                     ENDIF
                  ENDIF
               ENDDO
            endif
         ENDDO
       ENDDO
C      Turn caching in dsigproc back off to avoid side effects.
       LAST_ICONF=-1

c     Cannot make a selection with all PDFs to zero, so we return now
	  IF(SUMPROB.eq.0.0d0) then
	    RETURN
	  endif
	  

C     Perform the selection
      CALL RANMAR(R)

C     It is important to cache the status before adding any entries to this grid in this
C     routine since it might change it
	  grouped_MC_grid_status = DS_get_dim_status('grouped_processes')
	  
	  IF (MC_grouped_subproc.and.grouped_MC_grid_status.eq.0) then
c       We must initialize the grid and probe all channels
        SUMPROB=0.0d0
C       Turn caching on in dsigproc to avoid too many calls to switchmom
        LAST_ICONF=0
        DO J=1,SYMCONF(0)
          DO I=1,MAXSPROC
            IF(init_mode.or.CONFSUB(I,SYMCONF(J)).NE.0) THEN		  
              DO K=1,2
                IF(K.EQ.1.OR.MIRRORPROCS(I))THEN			
                  IPROC=I
                  ICONF=J
                  IMIRROR=K
C                 The IMODE=5 computes the matrix_element only, without PDF convolution 
                  DSIG=DSIGPROC(PP,ICONF,IPROC,IMIRROR,SYMCONF,CONFSUB,WGT,5)			  
			      call map_3_to_1(J,I,K,MAXSPROC,2,Lmapped)
			      IF (SELPROC(K,I,J).ne.0.0d0) then
		            call DS_add_entry('grouped_processes',Lmapped,DSIG)
		          endif
                  IF(K.EQ.2)THEN
C                   Need to flip back x values
                    XDUM=XBK(1)
                    XBK(1)=XBK(2)
                    XBK(2)=XDUM
                    CM_RAP=-CM_RAP
                  ENDIF
		  if(init_mode) then
		    SELPROC(K,I,J) = 1d0
		  else
                    SELPROC(K,I,J) = DABS(DSIG*SELPROC(K,I,J))
		  endif
                  SUMPROB = SUMPROB + SELPROC(K,I,J) 
		        ENDIF
              ENDDO
			ENDIF
          ENDDO
        ENDDO
C       Turn caching in dsigproc back off to avoid side effects.
        LAST_ICONF=-1
C       If these additional entries were enough to initialize the gird, then update it
C       To do this check we must *not* used the cached varianble grouped_MC_grid_status
		IF(DS_get_dim_status('grouped_processes').ge.1) then
		  CALL DS_update_grid('grouped_processes')
		  call reset_cumulative_variable()
		endif
      ENDIF

c     If we are still initializing the grid or simply not using one at all, then we pick a point based on PDF only.
	  IF (.not.MC_grouped_subproc.or.grouped_MC_grid_status.eq.0) then
	  R=R*SUMPROB
      ICONF=0
      IPROC=0
      TOTWGT=0D0
      DO J=1,SYMCONF(0)
        DO I=1,MAXSPROC
	  IF(init_mode.or.CONFSUB(I,SYMCONF(J)).NE.0) THEN
          DO K=1,2
            TOTWGT=TOTWGT+SELPROC(K,I,J)
            IF(R.LT.TOTWGT)THEN
                IPROC=I
                ICONF=J
                IMIRROR=K
                GOTO 50
             ENDIF
          ENDDO
	  ENDIF
        ENDDO
      ENDDO
 50   CONTINUE

      IF(IPROC.EQ.0) RETURN


C     Update weigth w.r.t SELPROC normalized to selection probability

      WGT=WGT*(SUMPROB/SELPROC(IMIRROR,IPROC,ICONF))

      ELSE
C       We are using the grouped_processes grid and it is initialized.
        call DS_get_point('grouped_processes',R,Lmapped,MC_grouped_proc_jacobian,'norm',(/'PDF_convolution'/))
		WGT=WGT*MC_grouped_proc_jacobian
		call map_1_to_3(Lmapped,MAXSPROC,2,ICONF,IPROC,IMIRROR)
	  ENDIF

C     Redo clustering to ensure consistent with final IPROC
      CUTSDONE=.FALSE.

	  IF(grouped_MC_grid_status.eq.0) then
C       If we were in the initialization phase of the grid for MC over grouped processes, we must instruct the matrix<i> subroutine not to add again an entry in the grid for this PS point at the call DSIGPROC just below.
        ALLOW_HELICITY_GRID_ENTRIES = .False.
      endif

C     Call DSIGPROC to calculate sigma for process
      DSIG=DSIGPROC(PP,ICONF,IPROC,IMIRROR,SYMCONF,CONFSUB,WGT,IMODE)
c     Reset ALLOW_HELICITY_GRID_ENTRIES
	  ALLOW_HELICITY_GRID_ENTRIES = .True. 

c     Below is how one would go about adding each point to the grouped_processes grid
C     However, keeping only the initialization grid is better because in that case all grouped ME's
C     were computed with the same kinematics. For this reason, the code below remains commented.
c	  IF(grouped_MC_grid_status.ge.1) then
c	    call map_3_to_1(ICONF,IPROC,IMIRROR,MAXSPROC,2,Lmapped)
c		call DS_add_entry('grouped_processes',Lmapped,(DSIG/SELPROC(IMIRROR,IPROC,ICONF)))
c	  ENDIF

      IF(DSIG.GT.0d0)THEN
C     Update summed weight and number of events
        SUMWGT(IMIRROR,IPROC,ICONF)=SUMWGT(IMIRROR,IPROC,ICONF)+DABS(DSIG*WGT)
        NUMEVTS(IMIRROR,IPROC,ICONF)=NUMEVTS(IMIRROR,IPROC,ICONF)+1
      ENDIF

      RETURN
      END

      FUNCTION DSIGPROC(PP,ICONF,IPROC,IMIRROR,SYMCONF,CONFSUB,WGT,IMODE)
C     ****************************************************
C     RETURNS DIFFERENTIAL CROSS SECTION 
C     FOR A PROCESS
C     Input:
C     pp    4 momentum of external particles
C     wgt   weight from Monte Carlo
C     imode 0 run, 1 init, 2 reweight, 3 finalize
C     Output:
C     Amplitude squared and summed
C     ****************************************************

      IMPLICIT NONE
      
      INCLUDE 'genps.inc'
      include 'maxconfigs.inc'
      INCLUDE 'nexternal.inc'
      INCLUDE 'maxamps.inc'
      include 'vector.inc' ! defines VECSIZE_MEMMAX
      include 'coupl.inc' ! needs VECSIZE_MEMMAX (defined in vector.inc)
      INCLUDE 'run.inc'
C     
C     ARGUMENTS 
C     
      DOUBLE PRECISION DSIGPROC
      DOUBLE PRECISION PP(0:3,NEXTERNAL), WGT
      INTEGER ICONF,IPROC,IMIRROR,IMODE
      INTEGER SYMCONF(0:LMAXCONFIGS)
      INTEGER CONFSUB(MAXSPROC,LMAXCONFIGS)
C     
C     GLOBAL VARIABLES
C     
C     SUBDIAG is vector of diagram numbers for this config
C     IB gives which beam is which (for mirror processes)
      INTEGER SUBDIAG(MAXSPROC),IB(2)
      COMMON/TO_SUB_DIAG/SUBDIAG,IB
C     ICONFIG has this config number
      INTEGER MAPCONFIG(0:LMAXCONFIGS), ICONFIG
      COMMON/TO_MCONFIGS/MAPCONFIG, ICONFIG
C     CM_RAP has parton-parton system rapidity
      DOUBLE PRECISION CM_RAP
      LOGICAL SET_CM_RAP
      COMMON/TO_CM_RAP/SET_CM_RAP,CM_RAP
c     To limit the number of calls to switchmom, use in DSIGPROC the cached variable last_iconfig. When set to -1, it ignores caching (to prevent undesired effect if this subroutine is called from elsewhere) and when set to 0, it resets the cache.
	  INTEGER LAST_ICONF
	  DATA LAST_ICONF/-1/
	  COMMON/TO_LAST_ICONF/LAST_ICONF
C  
C EXTERNAL FUNCTIONS
C  
%(dsig_def_line)s
      LOGICAL PASSCUTS
C     
C     LOCAL VARIABLES 
C     
      DOUBLE PRECISION P1(0:3,NEXTERNAL),xdum
      INTEGER I,J,K,JC(NEXTERNAL)
      INTEGER PERMS(NEXTERNAL,LMAXCONFIGS)
      INCLUDE 'symperms.inc'
	  SAVE P1,JC

      if (LAST_ICONF.eq.-1.or.LAST_ICONF.ne.ICONF) then

        ICONFIG=SYMCONF(ICONF)
        DO I=1,MAXSPROC
          SUBDIAG(I) = CONFSUB(I,SYMCONF(ICONF))
        ENDDO

C       Set momenta according to this permutation
        CALL SWITCHMOM(PP,P1,PERMS(1,MAPCONFIG(ICONFIG)),JC,NEXTERNAL)

	    if (LAST_ICONF.ne.-1) then
		  LAST_ICONF = ICONF
		endif
	  endif

      IB(1)=1
      IB(2)=2

      IF(IMIRROR.EQ.2)THEN
C       Flip momenta (rotate around x axis)
        DO I=1,NEXTERNAL
          P1(2,I)=-P1(2,I)
          P1(3,I)=-P1(3,I)
        ENDDO
C       Flip beam identity
        IB(1)=2
        IB(2)=1
C       Flip x values (to get boost right)
        XDUM=XBK(1)
        XBK(1)=XBK(2)
        XBK(2)=XDUM
C       Flip CM_RAP (to get rapidity right)
        CM_RAP=-CM_RAP
      ENDIF

      DSIGPROC=0D0

c     not needed anymore ... can be removed ... set for debugging only   
c     IF (.not.PASSCUTS(P1)) THEN
c       stop 1
c     endif

c     set the running scale 
c     and update the couplings accordingly
      IF (VECSIZE_MEMMAX.LE.1) THEN ! no-vector (NB not VECSIZE_USED!)
      call update_scale_coupling(pp, wgt)
      endif




      if (IMODE.eq.0d0.and.nb_pass_cuts.lt.2**12)then
        nb_pass_cuts = nb_pass_cuts + 1
      endif
%(call_dsig_proc_lines)s
c      ENDIF

	  IF (LAST_ICONF.ne.-1.and.IMIRROR.EQ.2) then
C       Flip back local momenta P1 if cached
        DO I=1,NEXTERNAL
          P1(2,I)=-P1(2,I)
          P1(3,I)=-P1(3,I)
        ENDDO
	  endif

      RETURN

      END
      
C     ccccccccccccccccccccccccc      
C      vectorize version
C     ccccccccccccccccccccccccc

      subroutine DSIGPROC_VEC(ALL_P,ALL_XBK,ALL_Q2FACT,ALL_CM_RAP,ICONF_VEC,IPROC,IMIRROR_VEC,SYMCONF,CONFSUB,ALL_WGT,IMODE,all_out,VECSIZE_USED)
C     ****************************************************
C     RETURNS DIFFERENTIAL CROSS SECTION 
C     FOR A PROCESS
C     Input:
C     pp    4 momentum of external particles
C     wgt   weight from Monte Carlo
C     imode 0 run, 1 init, 2 reweight, 3 finalize
C     Output:
C     Amplitude squared and summed
C     ****************************************************

      IMPLICIT NONE
      
      INCLUDE 'genps.inc'
      include 'maxconfigs.inc'
      INCLUDE 'nexternal.inc'
      INCLUDE 'maxamps.inc'
      include 'vector.inc' ! defines VECSIZE_MEMMAX/WARP_SIZE
      include 'coupl.inc' ! needs VECSIZE_MEMMAX (defined in vector.inc)
      INCLUDE 'run.inc'
C     
C     ARGUMENTS 
C
      double precision all_p(4*maxdim/3+14,VECSIZE_MEMMAX)
      double precision all_xbk(2, VECSIZE_MEMMAX)
      double precision all_q2fact(2, VECSIZE_MEMMAX)
      double precision all_cm_rap(VECSIZE_MEMMAX)
      double precision all_wgt(VECSIZE_MEMMAX)
      double precision all_out(VECSIZE_MEMMAX)
      DOUBLE PRECISION DSIGPROC
      INTEGER ICONF,IPROC,IMIRROR,IMODE
      INTEGER ICONF_VEC(NB_WARP), IMIRROR_VEC(NB_WARP)
      INTEGER CURR_WARP, IWARP, NB_WARP_USED
      INTEGER SYMCONF(0:LMAXCONFIGS)
      INTEGER CONFSUB(MAXSPROC,LMAXCONFIGS)
      INTEGER VECSIZE_USED
C     
C     GLOBAL VARIABLES
C     
C     SUBDIAG is vector of diagram numbers for this config
C     IB gives which beam is which (for mirror processes)
      INTEGER SUBDIAG(MAXSPROC),IB(2)
      COMMON/TO_SUB_DIAG/SUBDIAG,IB
C     ICONFIG has this config number
      INTEGER MAPCONFIG(0:LMAXCONFIGS), ICONFIG
      COMMON/TO_MCONFIGS/MAPCONFIG, ICONFIG
C     CM_RAP has parton-parton system rapidity
      DOUBLE PRECISION CM_RAP
      LOGICAL SET_CM_RAP
      COMMON/TO_CM_RAP/SET_CM_RAP,CM_RAP
c     To limit the number of calls to switchmom, use in DSIGPROC the cached variable last_iconfig. When set to -1, it ignores caching (to prevent undesired effect if this subroutine is called from elsewhere) and when set to 0, it resets the cache.
	  INTEGER LAST_ICONF
	  DATA LAST_ICONF/-1/
	  COMMON/TO_LAST_ICONF/LAST_ICONF
	  integer ivec
C  
C EXTERNAL FUNCTIONS
C  
%(dsig_def_line)s
      LOGICAL PASSCUTS
C     
C     LOCAL VARIABLES 
C     
      DOUBLE PRECISION ALL_P1(0:3,NEXTERNAL,VECSIZE_MEMMAX),xdum
      INTEGER I,J,K,JC(NEXTERNAL)
      INTEGER PERMS(NEXTERNAL,LMAXCONFIGS)
      INCLUDE 'symperms.inc'
	  SAVE ALL_P1,JC

      IF(LAST_ICONF.ne.-1) then
      	stop 25
      endif
      LAST_ICONF = 0 
      IWARP = 0 ! position within the current warp
      CURR_WARP = 1 ! current_warp used
      do IVEC=1, VECSIZE_USED
        IWARP = IWARP + 1
	if (IWARP.eq.1) then
           if (LAST_ICONF.eq.-1.or.LAST_ICONF.ne.ICONF_VEC(CURR_WARP)) then
              ICONFIG=SYMCONF(ICONF_VEC(CURR_WARP))
              DO I=1,MAXSPROC
                 SUBDIAG(I) = CONFSUB(I,SYMCONF(ICONF_VEC(CURR_WARP)))
              ENDDO
	  endif
c	  ICONF = ICONF_VEC(CURR_WARP)
c	  IMIRROR = IMIRROR_VEC(CURR_WARP)
	endif	
C       Set momenta according to this permutation
	CALL SWITCHMOM(ALL_P(1,IVEC),All_P1(0,1,IVEC),PERMS(1,MAPCONFIG(ICONFIG)),JC,NEXTERNAL)
	LAST_ICONF = ICONF_VEC(CURR_WARP)
	if (IWARP.eq.WARP_SIZE) then
	CURR_WARP = CURR_WARP + 1 
	IWARP = 0 
	endif
      enddo
      LAST_ICONF=-1

      NB_WARP_USED = VECSIZE_USED / WARP_SIZE
      IF( NB_WARP_USED * WARP_SIZE .NE. VECSIZE_USED ) THEN
        WRITE(*,*) 'ERROR: NB_WARP_USED * WARP_SIZE .NE. VECSIZE_USED',
     &    NB_WARP_USED, WARP_SIZE, VECSIZE_USED
        STOP
      ENDIF

      DO CURR_WARP=1,NB_WARP_USED
      	   IB(1)=0 ! This is set in auto_dsigX. set it to zero to create segfault if used at wrong time
      	   IB(2)=0 ! Same
	   IMIRROR = IMIRROR_VEC(CURR_WARP)
	   IF(IMIRROR.EQ.2)THEN
C       Flip momenta (rotate around x axis)
	     DO IVEC = (CURR_WARP-1)*WARP_SIZE+1,CURR_WARP*WARP_SIZE
	        DO I=1,NEXTERNAL
                   ALL_P1(2,I, IVEC)=-ALL_P1(2,I,IVEC)
                   ALL_P1(3,I, IVEC)=-ALL_P1(3,I,IVEC)
	        enddo
                XDUM=ALL_XBK(1, ivec)
	        ALL_XBK(1, ivec) = ALL_XBK(2, ivec)
	        ALL_XBK(2, ivec) = XDUM
	        all_cm_rap(ivec) = - all_cm_rap(ivec)
		IB(1) = 0
		IB(2) = 0
C       Flip beam identity -> moved to auto_dsigX (since depend of the warp)
             ENDDO
      
           ENDIF
      ENDDO


      ALL_OUT(:)=0D0

      DO IVEC=1,VECSIZE_USED
        if (IMODE.eq.0d0.and.nb_pass_cuts.lt.2**12.and.all_wgt(ivec).ne.0d0)then
          nb_pass_cuts = nb_pass_cuts + 1
        endif
      ENDDO
      
%(call_dsig_proc_lines_vec)s

C    FLIPPING BACK IF NEEDED
      DO CURR_WARP=1,NB_WARP_USED
      	  IF (IMIRROR_VEC(CURR_WARP).eq.2) then
	     DO IVEC = (CURR_WARP-1)*WARP_SIZE+1,CURR_WARP*WARP_SIZE
	        DO I=1,NEXTERNAL	
                  ALL_P1(2,I,IVEC)=-ALL_P1(2,I,IVEC)
                  ALL_P1(3,I,IVEC)=-ALL_P1(3,I,IVEC)
		ENDDO
      	     ENDDO
	  ENDIF
      ENDDO

      RETURN

      END
      

C     -----------------------------------------
C     Subroutine to map three positive integers
C     I, J and K with upper bounds J_bound and
C     K_bound to a one_dimensional
C     index L
C     -----------------------------------------

      subroutine map_3_to_1(I,J,K,J_bound,K_bound,L)
	  implicit none
	  integer, intent(in)  :: I,J,K,J_bound,K_bound
	  integer, intent(out) :: L

	  L = I*(J_bound*(K_bound+1)+K_bound+1)+J*(K_bound+1)+K

	  end subroutine map_3_to_1

C     -----------------------------------------
C     Subroutine to map back the positive 
C     integer L to the three integers 
C     I, J and K with upper bounds
C     J_bound and K_bound.
C     -----------------------------------------

      subroutine map_1_to_3(L,J_bound,K_bound,I,J,K)
	  implicit none
	  integer, intent(out)  :: I,J,K
	  integer, intent(in)   :: L, J_bound, K_bound
	  integer               :: L_run

      L_RUN = L
      I = L_RUN/(J_BOUND*(K_BOUND+1)+K_BOUND+1)
      L_RUN = L_RUN - I*((J_BOUND*(K_BOUND+1)+K_BOUND+1))
      J = L_RUN/(K_BOUND+1)
      L_RUN = L_RUN - J*(K_BOUND+1)
      K  = L_RUN

	  end subroutine map_1_to_3


C
C     Functionality to handling grid
C
%(read_write_good_hel)s


     SUBROUTINE PRINT_ZERO_AMP()

     %(print_zero_amp)s
     return
     end


     integer function get_nhel(hel,partid)
      implicit none
      integer hel,partid
      write(*,*) "this type of pdf is not support with group_subprocess=True. regenerate process with: set group_subprocesses false"
      stop 5
      return
      end


    subroutine select_color(rcol, jamp2, iconfig, iproc, icol)
    implicit none
    include 'maxamps.inc' ! for the definition of maxflow
    include 'coloramps.inc' ! set the coloramps
c    
c   argument IN
c
	double precision rcol ! random number
	double precision jamp2(0:maxflow)
	integer iconfig ! amplitude selected
	integer iproc ! matrix element selected
c	
c   argument OUT
c
       integer icol
c
c    local
c
	integer nc ! number of assigned color in jamp2
	logical is_LC
	integer maxcolor
	double precision targetamp(0:maxflow)
	integer i,j
	double precision xtarget

      nc = int(jamp2(0))
      is_LC = .true.
      maxcolor=0
      targetamp(0) = 0d0
      if(nc.eq.0)then
      icol = 0
      return
      endif
      do i=1,nc
      	  if(icolamp(i,iconfig,iproc))then
        	  targetamp(i) = targetamp(i-1) + jamp2(i)
          else	       
        	  targetamp(i) = targetamp(i-1)
          endif
      enddo	  

c     ensure that at least one leading color is different of zero if not allow
c     all subleading color.
      if (targetamp(nc).eq.0)then
       is_LC = .false.
       do icol =1,nc
           targetamp(icol) = jamp2(icol)+targetamp(icol-1)
       enddo
      endif

      xtarget=rcol*targetamp(nc)

      icol = 1
      do while (targetamp(icol) .lt. xtarget .and. icol .lt. nc)
         icol = icol + 1
      enddo

       return
       end    

       subroutine get_helicities(iproc, ihel, nhel)
       implicit none
       include 'nexternal.inc'
       integer iproc
       integer ihel
       integer nhel(nexternal)
       integer i
       %(definition_of_local_get_nhel)s
       
       %(call_to_local_get_helicities)s

       return
       end
