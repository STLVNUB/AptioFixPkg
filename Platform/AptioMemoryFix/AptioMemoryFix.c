/**

 UEFI driver for enabling loading of macOS without memory relocation.

 by dmazar

 **/

#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>

#include <Guid/GlobalVariable.h>

#include <Protocol/AptioMemoryFixProtocol.h>

#include "Config.h"
#include "BootArgs.h"
#include "BootFixes.h"
#include "CustomSlide.h"
#include "HashServices/HashServices.h"
#include "RtShims.h"
#include "ServiceOverrides.h"
#include "UnicodeCollation/UnicodeCollationEng.h"
#include "VMem.h"

//
// One could discover AptioMemoryFix with this protocol
//
STATIC APTIOMEMORYFIX_PROTOCOL mAptioMemoryFixProtocol = {
  APTIOMEMORYFIX_PROTOCOL_REVISION
};

/** Entry point. Installs our StartImage override.
 * All other stuff will be installed from there when boot.efi is started.
 */
EFI_STATUS
EFIAPI
AptioMemoryFixEntrypoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;
  VOID        *Interface;
  EFI_HANDLE  Handle = NULL;

  Status = gBS->LocateProtocol (
    &gAptioMemoryFixProtocolGuid,
    NULL,
    &Interface
    );

  if (!EFI_ERROR (Status)) {
    //
    // In case for whatever reason one tried to reload the driver.
    //
    return EFI_ALREADY_STARTED;
  }

  Status = gBS->InstallProtocolInterface (
    &Handle,
    &gAptioMemoryFixProtocolGuid,
    EFI_NATIVE_INTERFACE,
    &mAptioMemoryFixProtocol
    );

  if (EFI_ERROR (Status)) {
    PrintScreen (L"AMF: protocol install failure - %r\n", Status);
    return Status;
  }

#if APTIOFIX_UNICODE_COLLATION_FIX == 1
  Status = InitializeUnicodeCollationEng (ImageHandle, SystemTable);
  if (EFI_ERROR (Status)) {
    PrintScreen (L"AMF: collation install failure - %r\n", Status);
    return Status;
  }
#endif

#if APTIOFIX_HASH_SERVICES_FIX == 1
  Status = InitializeHashServices (ImageHandle, SystemTable);
  if (EFI_ERROR (Status)) {
    PrintScreen (L"AMF: hash install failure - %r\n", Status);
    return Status;
  }
#endif

#if APTIOFIX_HASH_SERVICES_TEST == 1
  if (!EFI_ERROR (Status)) {
    HSTestImpl ();
  }
#endif

  //
  // Init VMem memory pool - will be used after ExitBootServices
  //
  Status = VmAllocateMemoryPool ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // Install permanent shims and overrides
  //
  InstallRtShims (GetVariableCustomSlide);
  InstallBsOverrides ();
  InstallRtOverrides ();

  return EFI_SUCCESS;
}
