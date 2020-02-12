# 오픈 PS2 로더

Copyright 2013, Ifcaro & jimmikaelkael  
Academic 무료 라이센스 버전 3.0에 따라 라이센스 부여  
자세한 내용은 LICENSE 파일을 검토하십시오.  

## 소개

오픈 PS2 로더 (OPL)는 PS2 와 PS3 장치를 위한 100% 오픈 소스 게임 및 응용 프로그램
로더입니다. USB 대용량 저장 장치, SMB 공유, 플레이스테이션 2 HDD 장치의 세 가지 
범주의 장치를 지원합니다. USB 장치, SMB 공유는 USBExtreme 및 \*.ISO 형식을 지원하는
반면 PS2 HDD는 HDLoader 형식을 지원합니다. 현재 가장 호환성 좋은 홈브류 로더입니다.

OPL은 지속적으로 개발되며 누구나 오픈 소스 특성으로 인해 프로젝트 개선에 기여할 수
있습니다.

오픈 PS2 로더 포럼을 방문 할 수 있습니다:  

http://www.ps2-home.com/forum/viewforum.php?f=13

http://psx-scene.com/forums/official-open-ps2-loader-forum/ 

https://www.psx-place.com/forums/open-ps2-loader-opl.77/

다음에서 호환성 게임 문제를보고 할 수 있습니다:

http://www.ps2-home.com/forum/viewtopic.php?f=13&t=175

https://www.psx-place.com/threads/open-ps2-loader-game-bug-reports.19401/

업데이트 된 호환성 목록을 보려면 다음 사이트의 OPL-CL 사이트를 방문하십시오:  

http://www.ps2-home.com/forum/page/opl-game-compatibility-list-1

http://sx.sytes.net/oplcl/games.aspx  

## 개정 유형

오픈 PS2 로더 번들에는 동일한 OPL 버전의 여러 유형이 포함되었습니다. 이러한
유형에는 다소의 기능이 포함되어 있습니다

| 유형 (조합 가능)            | 설명                                                                              |
| --------------------------- | --------------------------------------------------------------------------------- |
| "Normal" (접미사 없음)      | 추가 기능 없이 정기적인 기본 OPL 릴리스. 일명 바닐라 빌드.                        |
| "Childproof"                | 일부 컨트롤이 비활성화 된 OPL 릴리스 (예: 쓰기 작업).                             |
| "VMC"                       | 가상 메모리 카드 (VMC)를 지원하는 OPL 릴리스.                                     |
| "GSM"                       | GSM이 통합 된 OPL 릴리즈.                                                         |
| "PS2RD"                     | PS2RD 치트 엔진 내장 기능이 있는 OPL 릴리스.                                      | 

## 사용하는 방법

OPL은 HDD, SMB, USB 모드에서 다음 디렉토리 트리 구조를 사용합니다:  

| 폴더   | 설명        | 모드  |
| ------ | ----------- | ----- |
| "CD" | CD 미디어의 게임 - 예: 파란색 하단 디스크) | USB 와 SMB |
| "DVD" | USB 또는 SMB에서 NTFS 파일 시스템을 사용하는 경우 DVD5 및 DVD9 이미지의 경우; USB 또는 SMB에서 FAT32 파일 시스템을 사용하는 경우 DVD9 이미지를 분할하여 장치 루트에 배치해야 함 | USB 와 SMB |
| "VMC" | 가상 메모리 카드 이미지의 경우 - 8MB에서 최대 64MB | 모두 |
| "CFG" | 게임 별 구성 파일 저장 | 모두 |
| "ART" | 게임 아트 이미지 | all |
| "THM" | 테마 지원 | all |
| "LNG" | 번역 지원 | 모두 |
| "CHT" | 치트 파일 | 모두 |
| "CFG-DEV" | OPL 개발 빌드에서 사용될 때 게임 별 구성 파일 저장 - 베타 빌드 | 모두 |

OPL은 처음 시작할 때 위의 디렉토리 구조를 자동으로 생성하고 즐겨 사용하는 장치를
활성화합니다. HDD 사용자의 경우 128Mb +OPL 파티션이 생성됩니다 (필요한 경우
uLaunchELF를 사용하여 확대할 수 있음).

## USB

USB의 게임 파일은 파일 또는 전체 드라이브로 완벽하게 조각 모음을 수행해야하며
FAT32 파일 시스템의 4GB 제한을 피하기 위해 듀얼 레이어 DVD9 이미지를 분할해야
합니다. 최상의 조각 모음 결과를 얻으려면 Auslogics Disk Defrag을 권장합니다.

http://www.auslogics.com/en/software/disk-defrag/  

또한 게임을 USBUtil 2.0과 같은 USB Advance/Extreme 형식으로 변환하거나 분할하려면
PC 프로그램이 필요합니다.

## SMB

SMB 프로토콜로 게임을 로드하려면 호스트 컴퓨터 또는 NAS 장치에서 폴더
(예: PS2SMB)를 공유하고 전체 읽기 및 쓰기 권한이 있는지 확인해야합니다.
USB Advance/Extreme 형식은 선택 사항입니다. \*.ISO 이미지는 위의 폴더
구조를 사용하여 지원되며 SMB 장치가 NTFS 또는 EXT3/4 파일 시스템을
사용하는 경우 DVD9 이미지를 분할할 필요가 없다는 추가 보너스가 있습니다.

## HDD

PS2의 경우 최대 2TB의 48 비트 LBA 내장 HDD가 지원됩니다. HDLoader 또는 
uLaunchELF로 포맷해야합니다 (uLaunchELF 권장).

OPL을 시작하기 위해 실행 가능한 elf를 로드하는 기존 방법 중 하나를 사용할
수 있습니다.

PS3에는 원본 SwapMagic 3.6 이상 또는 3.8 디스크가 필요합니다 (현재 다른
옵션은 없습니다). PS3에 OPL을 로드하는 단계는 다음과 같습니다:

1. OPNPS2LD.ELF를 SMBOOT0.ELF로 이름을 바꿉니다.
2. SWAPMAGIC이라는 USB 장치의 루트에 폴더를 만들고 SMBOOT0.ELF를 복사하십시오.
3. PS3에서 SwapMagic을 시작하고 UP+L1을 누르면 오픈 PS2 로더가 시작됩니다.

SwapMagic에는 elf를 시작하기 위한 4 가지 형태가 있습니다.  

SMBOOT0.ELF = UP + L1  
SMBOOT1.ELF = UP + L2  
SMBOOT2.ELF = UP + R1  
SMBOOT3.ELF = UP + R2  

참고: PS3에서는 USB 및 SMB 모드만 지원됩니다.

## 개발을 위한 참고 사항

오픈 PS2 로더에는 최신 PS2SDK가 필요합니다:  

https://github.com/ps2dev/ps2sdk
