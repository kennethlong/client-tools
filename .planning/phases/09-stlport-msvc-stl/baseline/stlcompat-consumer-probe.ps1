$consumers = @(
  'src/engine/shared/library/sharedDebug/src/shared/LeakFinder.h',
  'src/engine/shared/library/sharedMath/src/shared/PositionVertexIndexer.h',
  'src/engine/shared/library/sharedUtility/src/shared/DataTable.h',
  'src/engine/shared/library/sharedNetwork/src/shared/Address.h',
  'src/engine/shared/library/sharedNetworkMessages/src/shared/common/GameNetworkMessage.cpp',
  'src/engine/shared/library/sharedMessageDispatch/src/shared/MessageManager.cpp',
  'src/engine/shared/library/sharedObject/src/shared/object/NetworkIdManager.h',
  'src/engine/shared/library/sharedSkillSystem/src/shared/SkillManager.h',
  'src/engine/shared/library/sharedGame/src/shared/space/ShipComponentType.cpp',
  'src/external/ours/library/localization/src/shared/LocalizationManager.h',
  'src/engine/client/library/clientGame/src/shared/appearance/LightsaberCollisionManager.cpp',
  'src/engine/client/library/clientUserInterface/src/shared/core/CuiSkillManager.cpp',
  'src/engine/client/library/clientObject/src/shared/appearance/ComponentAppearanceTemplate.cpp',
  'src/external/3rd/library/ui/src/shared/core/UILowerString.h',
  'src/external/3rd/library/ui/src/win32/UIBaseObject.cpp',
  'src/external/3rd/library/ui/src/win32/UIManager.cpp',
  'src/external/3rd/library/ui/src/win32/UIStlFwd.h',
  'src/external/3rd/application/UiBuilder/DefaultObjectPropertiesManager.h',
  'src/external/ours/application/LocalizationTool/src/shared/LocalizationData.h',
  'src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiExpMonitor.cpp'
)
foreach ($f in $consumers) {
  $path = "D:/Code/swg-client/$f"
  $dir = Split-Path -Parent $path
  $found = $null
  $hasInclude = $false
  $hasLink = $false
  $cmFound = ''
  while ($dir -and (Test-Path $dir)) {
    $cm = Join-Path $dir 'CMakeLists.txt'
    if (Test-Path $cm) {
      $cmContent = Get-Content $cm -Raw -ErrorAction SilentlyContinue
      $i = ($cmContent -match 'sharedFoundation/include/public')
      $l = ($cmContent -match 'target_link_libraries\([^)]*sharedFoundation[^)]*\)')
      if ($i -or $l) {
        $hasInclude = $i
        $hasLink = $l
        $cmFound = $cm
        $found = $cm
        break
      }
      # Found a CMakeLists but it does not link to sharedFoundation; record and keep walking up
      if (-not $cmFound) { $cmFound = $cm }
    }
    $parent = Split-Path -Parent $dir
    if ($parent -eq $dir) { break }
    $dir = $parent
  }
  if ($found) {
    Write-Host "$f"
    Write-Host "  CMakeLists: $cmFound"
    Write-Host "  hasInclude=$hasInclude  hasLink=$hasLink"
  } else {
    Write-Host "$f"
    Write-Host "  CMakeLists: $cmFound"
    Write-Host "  NO_RESOLUTION_PATH"
  }
}
