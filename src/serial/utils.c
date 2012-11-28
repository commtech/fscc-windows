/*++

Copyright (c) Microsoft Corporation

Module Name:

    utils.c

Abstract:

    This module contains code that perform queueing and completion
    manipulation on requests.  Also module generic functions such
    as error logging.

Environment:

    Kernel mode

--*/

#include "precomp.h"

#if defined(EVENT_TRACING)
#include "utils.tmh"
#endif

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGESRP0,SerialMemCompare)
#pragma alloc_text(PAGESRP0,SerialLogError)
#pragma alloc_text(PAGESRP0,SerialMarkHardwareBroken)
#endif // ALLOC_PRAGMA


VOID
SerialRundownIrpRefs(
    IN WDFREQUEST *CurrentOpRequest,
    IN WDFTIMER IntervalTimer,
    IN WDFTIMER TotalTimer,
    IN PSERIAL_DEVICE_EXTENSION PDevExt,
    IN LONG RefType
    );

static const PHYSICAL_ADDRESS SerialPhysicalZero = {0};

VOID
SerialPurgeRequests(
    IN WDFQUEUE QueueToClean,
    IN WDFREQUEST *CurrentOpRequest
    )

/*++

Routine Description:

    This function is used to cancel all queued and the current irps
    for reads or for writes. Called at DPC level.

Arguments:

    QueueToClean - A pointer to the queue which we're going to clean out.

    CurrentOpRequest - Pointer to a pointer to the current request.

Return Value:

    None.

--*/

{
    NTSTATUS status;
    PREQUEST_CONTEXT reqContext;

    WdfIoQueuePurge(QueueToClean, WDF_NO_EVENT_CALLBACK, WDF_NO_CONTEXT);

    //
    // The queue is clean.  Now go after the current if
    // it's there.
    //

    if (*CurrentOpRequest) {

        PFN_WDF_REQUEST_CANCEL CancelRoutine;

        reqContext = SerialGetRequestContext(*CurrentOpRequest);
        CancelRoutine = reqContext->CancelRoutine;
        //
        // Clear the common cancel routine but don't clear the reference because the
        // request specific cancel routine called below will clear the reference.
        //
        status = SerialClearCancelRoutine(*CurrentOpRequest, FALSE);
        if (NT_SUCCESS(status)) {
            //
            // Let us just call the CancelRoutine to start the next request.
            //
            if(CancelRoutine) {
                CancelRoutine(*CurrentOpRequest);
            }
        }
    }
}

VOID
SerialFlushRequests(
    IN WDFQUEUE QueueToClean,
    IN WDFREQUEST *CurrentOpRequest
    )

/*++

Routine Description:

    This function is used to cancel all queued and the current irps
    for reads or for writes. Called at DPC level.

Arguments:

    QueueToClean - A pointer to the queue which we're going to clean out.

    CurrentOpRequest - Pointer to a pointer to the current request.

Return Value:

    None.

--*/

{
    SerialPurgeRequests(QueueToClean,  CurrentOpRequest);

    //
    // Since purge puts the queue state to fail requests, we have to explicitly
    // change the queue state to accept requests.
    //
    WdfIoQueueStart(QueueToClean);

}


VOID
SerialGetNextRequest(
    IN WDFREQUEST               * CurrentOpRequest,
    IN WDFQUEUE                   QueueToProcess,
    OUT WDFREQUEST              * NextRequest,
    IN BOOLEAN                    CompleteCurrent,
    IN PSERIAL_DEVICE_EXTENSION   Extension
    )

/*++

Routine Description:

    This function is used to make the head of the particular
    queue the current request.  It also completes the what
    was the old current request if desired.

Arguments:

    CurrentOpRequest - Pointer to a pointer to the currently active
                   request for the particular work list.  Note that
                   this item is not actually part of the list.

    QueueToProcess - The list to pull the new item off of.

    NextIrp - The next Request to process.  Note that CurrentOpRequest
              will be set to this value under protection of the
              cancel spin lock.  However, if *NextIrp is NULL when
              this routine returns, it is not necessaryly true the
              what is pointed to by CurrentOpRequest will also be NULL.
              The reason for this is that if the queue is empty
              when we hold the cancel spin lock, a new request may come
              in immediately after we release the lock.

    CompleteCurrent - If TRUE then this routine will complete the
                      request pointed to by the pointer argument
                      CurrentOpRequest.

Return Value:

    None.

--*/

{
    WDFREQUEST       oldRequest = NULL;
    PREQUEST_CONTEXT reqContext;
    NTSTATUS         status;

    UNREFERENCED_PARAMETER(Extension);

    oldRequest = *CurrentOpRequest;
    *CurrentOpRequest = NULL;

    //
    // Check to see if there is a new request to start up.
    //

    status = WdfIoQueueRetrieveNextRequest(
                 QueueToProcess,
                 CurrentOpRequest
                 );

    if(!NT_SUCCESS(status)) {
        ASSERTMSG("WdfIoQueueRetrieveNextRequest failed",
                  status == STATUS_NO_MORE_ENTRIES);
    }

    *NextRequest = *CurrentOpRequest;

    if (CompleteCurrent) {

        if (oldRequest) {

            reqContext = SerialGetRequestContext(oldRequest);

            SerialCompleteRequest(oldRequest,
                                  reqContext->Status,
                                  reqContext->Information);
        }
    }
}

VOID
SerialTryToCompleteCurrent(
    IN PSERIAL_DEVICE_EXTENSION Extension,
    IN PFN_WDF_INTERRUPT_SYNCHRONIZE  SynchRoutine OPTIONAL,
    IN NTSTATUS StatusToUse,
    IN WDFREQUEST *CurrentOpRequest,
    IN WDFQUEUE QueueToProcess OPTIONAL,
    IN WDFTIMER IntervalTimer OPTIONAL,
    IN WDFTIMER TotalTimer OPTIONAL,
    IN PSERIAL_START_ROUTINE Starter OPTIONAL,
    IN PSERIAL_GET_NEXT_ROUTINE GetNextRequest OPTIONAL,
    IN LONG RefType
    )

/*++

Routine Description:

    This routine attempts to remove all of the reasons there are
    references on the current read/write.  If everything can be completed
    it will complete this read/write and try to start another.

    NOTE: This routine assumes that it is called with the cancel
          spinlock held.

Arguments:

    Extension - Simply a pointer to the device extension.

    SynchRoutine - A routine that will synchronize with the isr
                   and attempt to remove the knowledge of the
                   current request from the isr.  NOTE: This pointer
                   can be null.

    IrqlForRelease - This routine is called with the cancel spinlock held.
                     This is the irql that was current when the cancel
                     spinlock was acquired.

    StatusToUse - The request's status field will be set to this value, if
                  this routine can complete the request.


Return Value:

    None.

--*/

{
    PREQUEST_CONTEXT reqContext;

    ASSERTMSG("SerialTryToCompleteCurrent: CurrentOpRequest is NULL", *CurrentOpRequest);

     reqContext = SerialGetRequestContext(*CurrentOpRequest);

    if(RefType == SERIAL_REF_ISR || RefType == SERIAL_REF_XOFF_REF) {
        //
        // We can decrement the reference to "remove" the fact
        // that the caller no longer will be accessing this request.
        //

        SERIAL_CLEAR_REFERENCE(
            reqContext,
            RefType
            );
    }

    if (SynchRoutine) {

        WdfInterruptAcquireLock (Extension->WdfInterrupt);
        SynchRoutine(Extension->WdfInterrupt, Extension);
        WdfInterruptReleaseLock (Extension->WdfInterrupt);

    }

    //
    // Try to run down all other references to this request.
    //

    SerialRundownIrpRefs(
        CurrentOpRequest,
        IntervalTimer,
        TotalTimer,
        Extension,
        RefType
        );

    if(StatusToUse == STATUS_CANCELLED) {
        //
        // This function is called from a cancelroutine. So mark
        // the request as cancelled. We need to do this because
        // we may not complete the request below if somebody
        // else has a reference to it.
        // This state variable was added to avoid calling
        // WdfRequestMarkCancelable second time on a request that
        // has cancelled but wasn't completed in the cancel routine.
        //
        reqContext->Cancelled = TRUE;
    }

    //
    // See if the ref count is zero after trying to complete everybody else.
    //

    if (!SERIAL_REFERENCE_COUNT(reqContext)) {

        WDFREQUEST newRequest;


        //
        // The ref count was zero so we should complete this
        // request.
        //
        // The following call will also cause the current request to be
        // completed.
        //

        reqContext->Status = StatusToUse;

        if (StatusToUse == STATUS_CANCELLED) {

            reqContext->Information = 0;

        }

        if (GetNextRequest) {

            GetNextRequest(
                CurrentOpRequest,
                QueueToProcess,
                &newRequest,
                TRUE,
                Extension
                );

            if (newRequest) {

                Starter(Extension);

            }

        } else {

            WDFREQUEST oldRequest = *CurrentOpRequest;

            //
            // There was no get next routine.  We will simply complete
            // the request.  We should make sure that we null out the
            // pointer to the pointer to this request.
            //

            *CurrentOpRequest = NULL;

            SerialCompleteRequest(oldRequest,
                                  reqContext->Status,
                                  reqContext->Information);
        }

    } else {


    }

}


VOID
SerialEvtIoStop(
    IN WDFQUEUE                 Queue,
    IN WDFREQUEST               Request,
    IN ULONG                    ActionFlags
    )
/*++

Routine Description:

     This callback is invoked for every request pending in the driver (not queue) -
     in-flight request. The Action parameter tells us why the callback is invoked -
     because the device is being stopped, removed or suspended. In this
     driver, we have told the framework not to stop or remove when there
     are pending requests, so only reason for this callback is when the system is
     suspending.

Arguments:

    Queue - Queue the request currently belongs to
    Request - Request that is currently out of queue and being processed by the driver
    Action - Reason for this callback

Return Value:

    None. Acknowledge the request so that framework can contiue suspending the
    device.

--*/
{
    PREQUEST_CONTEXT reqContext;

    UNREFERENCED_PARAMETER(Queue);

    reqContext = SerialGetRequestContext(Request);

    SerialDbgPrintEx(TRACE_LEVEL_INFORMATION, DBG_WRITE,
                    "--> SerialEvtIoStop %x %p\n", ActionFlags, Request);

    //
    // System suspends all the timers before asking the driver to goto
    // sleep. So let us not worry about cancelling the timers. Also the
    // framework will disconnect the interrupt before calling our
    // D0Exit handler so we can be sure that nobody will touch the hardware.
    // So just acknowledge callback to say that we are okay to stop due to
    // system suspend. Please note that since we have taken a power reference
    // we will never idle out when there is an open handle. Also we have told
    // the framework to not stop for resource rebalancing or remove when there are
    // open handles, so let us not worry about that either.
    //
    if (ActionFlags & WdfRequestStopRequestCancelable) {
        PFN_WDF_REQUEST_CANCEL cancelRoutine;

        //
        // Request is in a cancelable state. So unmark cancelable before you
        // acknowledge. We will mark the request cancelable when we resume.
        //
        cancelRoutine = reqContext->CancelRoutine;

        SerialClearCancelRoutine(Request, TRUE);

        //
        // SerialClearCancelRoutine clears the cancel-routine. So set it back
        // in the context. We will need that when we resume.
        //
        reqContext->CancelRoutine = cancelRoutine;

        reqContext->MarkCancelableOnResume = TRUE;

        ActionFlags &= ~WdfRequestStopRequestCancelable;
    }

    ASSERT(ActionFlags == WdfRequestStopActionSuspend);

    WdfRequestStopAcknowledge(Request, FALSE); // Don't requeue the request

    SerialDbgPrintEx(TRACE_LEVEL_INFORMATION, DBG_WRITE,
                        "<-- SerialEvtIoStop \n");
}

VOID
SerialEvtIoResume(
    IN WDFQUEUE   Queue,
    IN WDFREQUEST Request
    )
/*++

Routine Description:

     This callback is invoked for every request pending in the driver - in-flight
     request - to notify that the hardware is ready for contiuing the processing
     of the request.

Arguments:

    Queue - Queue the request currently belongs to
    Request - Request that is currently out of queue and being processed by the driver

Return Value:

    None.

--*/
{
    PREQUEST_CONTEXT reqContext;

    UNREFERENCED_PARAMETER(Queue);

    SerialDbgPrintEx(TRACE_LEVEL_INFORMATION, DBG_WRITE,
            "--> SerialEvtIoResume %p \n", Request);

    reqContext = SerialGetRequestContext(Request);

    //
    // If we unmarked cancelable on suspend, let us mark it cancelable again.
    //
    if (reqContext->MarkCancelableOnResume) {
        SerialSetCancelRoutine(Request, reqContext->CancelRoutine);
        reqContext->MarkCancelableOnResume = FALSE;
    }

    SerialDbgPrintEx(TRACE_LEVEL_INFORMATION, DBG_WRITE,
        "<-- SerialEvtIoResume \n");
}

VOID
SerialRundownIrpRefs(
    IN WDFREQUEST *CurrentOpRequest,
    IN WDFTIMER IntervalTimer OPTIONAL,
    IN WDFTIMER TotalTimer OPTIONAL,
    IN PSERIAL_DEVICE_EXTENSION PDevExt,
    IN LONG RefType
    )

/*++

Routine Description:

    This routine runs through the various items that *could*
    have a reference to the current read/write.  It try's to remove
    the reason.  If it does succeed in removing the reason it
    will decrement the reference count on the request.

    NOTE: This routine assumes that it is called with the cancel
          spin lock held.

Arguments:

    CurrentOpRequest - Pointer to a pointer to current request for the
                   particular operation.

    IntervalTimer - Pointer to the interval timer for the operation.
                    NOTE: This could be null.

    TotalTimer - Pointer to the total timer for the operation.
                 NOTE: This could be null.

    PDevExt - Pointer to device extension

Return Value:

    None.

--*/


{
    PREQUEST_CONTEXT  reqContext;
    WDFREQUEST        request = *CurrentOpRequest;

    reqContext = SerialGetRequestContext(request);

    if(RefType == SERIAL_REF_CANCEL) {
        //
        // Caller is a cancel routine. So just clear the reference.
        //
        SERIAL_CLEAR_REFERENCE( reqContext,  SERIAL_REF_CANCEL );
        reqContext->CancelRoutine = NULL;

    } else {
        //
        // Try to clear the cancelable state.
        //
        SerialClearCancelRoutine(request, TRUE);
    }
    if (IntervalTimer) {

        //
        // Try to cancel the operations interval timer.  If the operation
        // returns true then the timer did have a reference to the
        // request.  Since we've canceled this timer that reference is
        // no longer valid and we can decrement the reference count.
        //
        // If the cancel returns false then this means either of two things:
        //
        // a) The timer has already fired.
        //
        // b) There never was an interval timer.
        //
        // In the case of "b" there is no need to decrement the reference
        // count since the "timer" never had a reference to it.
        //
        // In the case of "a", then the timer itself will be coming
        // along and decrement it's reference.  Note that the caller
        // of this routine might actually be the this timer, so
        // decrement the reference.
        //

        if (SerialCancelTimer(IntervalTimer, PDevExt)) {

            SERIAL_CLEAR_REFERENCE(
                reqContext,
                SERIAL_REF_INT_TIMER
                );

        } else if(RefType == SERIAL_REF_INT_TIMER) { // caller is the timer

            SERIAL_CLEAR_REFERENCE(
                reqContext,
                SERIAL_REF_INT_TIMER
                );
        }

    }

    if (TotalTimer) {

        //
        // Try to cancel the operations total timer.  If the operation
        // returns true then the timer did have a reference to the
        // request.  Since we've canceled this timer that reference is
        // no longer valid and we can decrement the reference count.
        //
        // If the cancel returns false then this means either of two things:
        //
        // a) The timer has already fired.
        //
        // b) There never was an total timer.
        //
        // In the case of "b" there is no need to decrement the reference
        // count since the "timer" never had a reference to it.
        //
        // In the case of "a", then the timer itself will be coming
        // along and decrement it's reference.  Note that the caller
        // of this routine might actually be the this timer, so
        // decrement the reference.
        //

        if (SerialCancelTimer(TotalTimer, PDevExt)) {

            SERIAL_CLEAR_REFERENCE(
                reqContext,
                SERIAL_REF_TOTAL_TIMER
                );

        } else if(RefType == SERIAL_REF_TOTAL_TIMER) { // caller is the timer

            SERIAL_CLEAR_REFERENCE(
                reqContext,
                SERIAL_REF_TOTAL_TIMER
                );
        }
    }
}


VOID
SerialStartOrQueue(
    IN PSERIAL_DEVICE_EXTENSION Extension,
    IN WDFREQUEST Request,
    IN WDFQUEUE QueueToExamine,
    IN WDFREQUEST *CurrentOpRequest,
    IN PSERIAL_START_ROUTINE Starter
    )

/*++

Routine Description:

    This routine is used to either start or queue any requst
    that can be queued in the driver.

Arguments:

    Extension - Points to the serial device extension.

    Request - The request to either queue or start.  In either
          case the request will be marked pending.

    QueueToExamine - The queue the request will be place on if there
                     is already an operation in progress.

    CurrentOpRequest - Pointer to a pointer to the request the is current
                   for the queue.  The pointer pointed to will be
                   set with to Request if what CurrentOpRequest points to
                   is NULL.

    Starter - The routine to call if the queue is empty.

Return Value:


--*/

{

    NTSTATUS status;
    PREQUEST_CONTEXT reqContext;
    WDF_REQUEST_PARAMETERS  params;

    reqContext = SerialGetRequestContext(Request);

    WDF_REQUEST_PARAMETERS_INIT(&params);

    WdfRequestGetParameters(
             Request,
             &params);

    //
    // If this is a write request then take the amount of characters
    // to write and add it to the count of characters to write.
    //

    if (params.Type == WdfRequestTypeWrite) {

        Extension->TotalCharsQueued += reqContext->Length;

    } else if ((params.Type == WdfRequestTypeDeviceControl) &&
               ((params.Parameters.DeviceIoControl.IoControlCode == IOCTL_SERIAL_IMMEDIATE_CHAR) ||
                (params.Parameters.DeviceIoControl.IoControlCode == IOCTL_SERIAL_XOFF_COUNTER))) {

        reqContext->IoctlCode = params.Parameters.DeviceIoControl.IoControlCode; // We need this in the destroy callback

        Extension->TotalCharsQueued++;

    }

    if (IsQueueEmpty(QueueToExamine) &&  !(*CurrentOpRequest)) {

        //
        // There were no current operation.  Mark this one as
        // current and start it up.
        //

        *CurrentOpRequest = Request;

        Starter(Extension);

        return;

    } else {

        //
        // We don't know how long the request will be in the
        // queue.  If it gets cancelled while waiting in the queue, we will
        // be notified by EvtCanceledOnQueue callback so that we can readjust
        // the lenght or free the buffer.
        //
        reqContext->Extension = Extension; // We need this in the destroy callback

        status = WdfRequestForwardToIoQueue(Request,  QueueToExamine);
        if(!NT_SUCCESS(status)) {
            SerialDbgPrintEx(TRACE_LEVEL_ERROR, DBG_READ, "WdfRequestForwardToIoQueue failed%X\n", status);
            ASSERTMSG("WdfRequestForwardToIoQueue failed ", FALSE);
            SerialCompleteRequest(Request, status, 0);
        }

        return;
    }
}

VOID
SerialEvtCanceledOnQueue(
    IN WDFQUEUE   Queue,
    IN WDFREQUEST Request
    )

/*++

Routine Description:

    Called when the request is cancelled while it's waiting
    on the queue. This callback is used instead of EvtCleanupCallback
    on the request because this one will be called with the
    presentation lock held.


Arguments:

    Queue - Queue in which the request currently waiting
    Request - Request being cancelled


Return Value:

    None.

--*/

{
    PSERIAL_DEVICE_EXTENSION extension = NULL;
    PREQUEST_CONTEXT reqContext;

    UNREFERENCED_PARAMETER(Queue);

    reqContext = SerialGetRequestContext(Request);

    extension = reqContext->Extension;

    //
    // If this is a write request then take the amount of characters
    // to write and subtract it from the count of characters to write.
    //

    if (reqContext->MajorFunction == IRP_MJ_WRITE) {

        extension->TotalCharsQueued -= reqContext->Length;

    } else if (reqContext->MajorFunction == IRP_MJ_DEVICE_CONTROL) {

        //
        // If it's an immediate then we need to decrement the
        // count of chars queued.  If it's a resize then we
        // need to deallocate the pool that we're passing on
        // to the "resizing" routine.
        //

        if (( reqContext->IoctlCode == IOCTL_SERIAL_IMMEDIATE_CHAR) ||
            (reqContext->IoctlCode ==  IOCTL_SERIAL_XOFF_COUNTER)) {

            extension->TotalCharsQueued--;

        } else if (reqContext->IoctlCode ==  IOCTL_SERIAL_SET_QUEUE_SIZE) {

            //
            // We shoved the pointer to the memory into the
            // the type 3 buffer pointer which we KNOW we
            // never use.
            //

            ASSERT(reqContext->Type3InputBuffer);

            ExFreePool(reqContext->Type3InputBuffer);

            reqContext->Type3InputBuffer = NULL;

        }

    }

    SerialCompleteRequest(Request, WdfRequestGetStatus(Request), 0);
}


NTSTATUS
SerialCompleteIfError(
    PSERIAL_DEVICE_EXTENSION extension,
    WDFREQUEST Request
    )

/*++

Routine Description:

    If the current request is not an IOCTL_SERIAL_GET_COMMSTATUS request and
    there is an error and the application requested abort on errors,
    then cancel the request.

Arguments:

    extension - Pointer to the device context

    Request - Pointer to the WDFREQUEST to test.

Return Value:

    STATUS_SUCCESS or STATUS_CANCELLED.

--*/

{

    WDF_REQUEST_PARAMETERS  params;
    NTSTATUS status = STATUS_SUCCESS;

    if ((extension->HandFlow.ControlHandShake &
         SERIAL_ERROR_ABORT) && extension->ErrorWord) {

        WDF_REQUEST_PARAMETERS_INIT(&params);

        WdfRequestGetParameters(
            Request,
            &params
            );


        //
        // There is a current error in the driver.  No requests should
        // come through except for the GET_COMMSTATUS.
        //

        if ((params.Type != WdfRequestTypeDeviceControl) ||
                    (params.Parameters.DeviceIoControl.IoControlCode !=  IOCTL_SERIAL_GET_COMMSTATUS)) {
            status = STATUS_CANCELLED;
            SerialCompleteRequest(Request, status, 0);
        }

    }

    return status;

}

NTSTATUS
SerialCreateTimersAndDpcs(
    IN PSERIAL_DEVICE_EXTENSION pDevExt
    )
/*++

Routine Description:

   This function creates all the timers and DPC objects. All the objects
   are associated with the WDFDEVICE and the callbacks are serialized
   with the device callbacks. Also these objects will be deleted automatically
   when the device is deleted, so there is no need for the driver to explicitly
   delete the objects.

Arguments:

   PDevExt - Pointer to the device extension for the device

Return Value:

    return NTSTATUS

--*/
{
   WDF_DPC_CONFIG dpcConfig;
   WDF_TIMER_CONFIG timerConfig;
   NTSTATUS status;
   WDF_OBJECT_ATTRIBUTES dpcAttributes;
   WDF_OBJECT_ATTRIBUTES timerAttributes;

   //
   // Initialize all the timers used to timeout operations.
   //
   //
   // This timer dpc is fired off if the timer for the total timeout
   // for the read expires.  It will cause the current read to complete.
   //

   WDF_TIMER_CONFIG_INIT(&timerConfig, SerialReadTimeout);

   timerConfig.AutomaticSerialization = TRUE;

   WDF_OBJECT_ATTRIBUTES_INIT(&timerAttributes);
   timerAttributes.ParentObject = pDevExt->WdfDevice;

   status = WdfTimerCreate(&timerConfig,
                           &timerAttributes,
                                    &pDevExt->ReadRequestTotalTimer);

   if (!NT_SUCCESS(status)) {
      SerialDbgPrintEx(TRACE_LEVEL_ERROR, DBG_PNP,  "WdfTimerCreate(ReadRequestTotalTimer) failed  [%#08lx]\n",   status);
      return status;
   }

   //
   // This dpc is fired off if the timer for the interval timeout
   // expires.  If no more characters have been read then the
   // dpc routine will cause the read to complete.  However, if
   // more characters have been read then the dpc routine will
   // resubmit the timer.
   //
   WDF_TIMER_CONFIG_INIT(&timerConfig,   SerialIntervalReadTimeout);

   timerConfig.AutomaticSerialization = TRUE;

   WDF_OBJECT_ATTRIBUTES_INIT(&timerAttributes);
   timerAttributes.ParentObject = pDevExt->WdfDevice;

   status = WdfTimerCreate(&timerConfig,
                           &timerAttributes,
                                        &pDevExt->ReadRequestIntervalTimer);

   if (!NT_SUCCESS(status)) {
      SerialDbgPrintEx(TRACE_LEVEL_ERROR, DBG_PNP,  "WdfTimerCreate(ReadRequestIntervalTimer) failed  [%#08lx]\n",   status);
      return status;
   }

   //
   // This dpc is fired off if the timer for the total timeout
   // for the write expires.  It will queue a dpc routine that
   // will cause the current write to complete.
   //
   //

   WDF_TIMER_CONFIG_INIT(&timerConfig,    SerialWriteTimeout);

   timerConfig.AutomaticSerialization = TRUE;

   WDF_OBJECT_ATTRIBUTES_INIT(&timerAttributes);
   timerAttributes.ParentObject = pDevExt->WdfDevice;

   status = WdfTimerCreate(&timerConfig,
                                &timerAttributes,
                                &pDevExt->WriteRequestTotalTimer);

   if (!NT_SUCCESS(status)) {
      SerialDbgPrintEx(TRACE_LEVEL_ERROR, DBG_PNP,  "WdfTimerCreate(WriteRequestTotalTimer) failed  [%#08lx]\n",   status);
      return status;
   }

   //
   // This dpc is fired off if the transmit immediate char
   // character times out.  The dpc routine will "grab" the
   // request from the isr and time it out.
   //
   WDF_TIMER_CONFIG_INIT(&timerConfig,   SerialTimeoutImmediate);

   timerConfig.AutomaticSerialization = TRUE;

   WDF_OBJECT_ATTRIBUTES_INIT(&timerAttributes);
   timerAttributes.ParentObject = pDevExt->WdfDevice;

   status = WdfTimerCreate(&timerConfig,
                           &timerAttributes,
                                        &pDevExt->ImmediateTotalTimer);

   if (!NT_SUCCESS(status)) {
      SerialDbgPrintEx(TRACE_LEVEL_ERROR, DBG_PNP,  "WdfTimerCreate(ImmediateTotalTimer) failed  [%#08lx]\n",   status);
      return status;
   }

   //
   // This dpc is fired off if the timer used to "timeout" counting
   // the number of characters received after the Xoff ioctl is started
   // expired.
   //

   WDF_TIMER_CONFIG_INIT(&timerConfig,   SerialTimeoutXoff);

   timerConfig.AutomaticSerialization = TRUE;

   WDF_OBJECT_ATTRIBUTES_INIT(&timerAttributes);
   timerAttributes.ParentObject = pDevExt->WdfDevice;

   status = WdfTimerCreate(&timerConfig,
                                    &timerAttributes,
                                    &pDevExt->XoffCountTimer);

    if (!NT_SUCCESS(status)) {
      SerialDbgPrintEx(TRACE_LEVEL_ERROR, DBG_PNP,  "WdfTimerCreate(XoffCountTimer) failed  [%#08lx]\n",   status);
      return status;
   }

   //
   // This dpc is fired off when a timer expires (after one
   // character time), so that code can be invoked that will
   // check to see if we should lower the RTS line when
   // doing transmit toggling.
   //
   WDF_TIMER_CONFIG_INIT(&timerConfig,  SerialInvokePerhapsLowerRTS);

   timerConfig.AutomaticSerialization = TRUE;

   WDF_OBJECT_ATTRIBUTES_INIT(&timerAttributes);
   timerAttributes.ParentObject = pDevExt->WdfDevice;

   status = WdfTimerCreate(&timerConfig,
                           &timerAttributes,
                                    &pDevExt->LowerRTSTimer);
    if (!NT_SUCCESS(status)) {
        SerialDbgPrintEx(TRACE_LEVEL_ERROR, DBG_PNP,  "WdfTimerCreate(LowerRTSTimer) failed  [%#08lx]\n",   status);
        return status;
    }

    //
    // Create a DPC to complete read requests.
    //

   WDF_DPC_CONFIG_INIT(&dpcConfig, SerialCompleteWrite);

   dpcConfig.AutomaticSerialization = TRUE;

   WDF_OBJECT_ATTRIBUTES_INIT(&dpcAttributes);
   dpcAttributes.ParentObject = pDevExt->WdfDevice;

   status = WdfDpcCreate(&dpcConfig,
                                    &dpcAttributes,
                                    &pDevExt->CompleteWriteDpc);
    if (!NT_SUCCESS(status)) {

        SerialDbgPrintEx(TRACE_LEVEL_ERROR, DBG_PNP,  "WdfDpcCreate(CompleteWriteDpc) failed  [%#08lx]\n",   status);
        return status;
    }


    //
    // Create a DPC to complete read requests.
    //

    WDF_DPC_CONFIG_INIT(&dpcConfig, SerialCompleteRead);

    dpcConfig.AutomaticSerialization = TRUE;

    WDF_OBJECT_ATTRIBUTES_INIT(&dpcAttributes);
    dpcAttributes.ParentObject = pDevExt->WdfDevice;

    status = WdfDpcCreate(&dpcConfig,
                                &dpcAttributes,
                                &pDevExt->CompleteReadDpc);

    if (!NT_SUCCESS(status)) {
        SerialDbgPrintEx(TRACE_LEVEL_ERROR, DBG_PNP,  "WdfDpcCreate(CompleteReadDpc) failed  [%#08lx]\n",   status);
        return status;
    }

    //
    // This dpc is fired off if a comm error occurs.  It will
    // cancel all pending reads and writes.
    //
    WDF_DPC_CONFIG_INIT(&dpcConfig, SerialCommError);

    dpcConfig.AutomaticSerialization = TRUE;

    WDF_OBJECT_ATTRIBUTES_INIT(&dpcAttributes);
    dpcAttributes.ParentObject = pDevExt->WdfDevice;

    status = WdfDpcCreate(&dpcConfig,
                                &dpcAttributes,
                                &pDevExt->CommErrorDpc);


    if (!NT_SUCCESS(status)) {

        SerialDbgPrintEx(TRACE_LEVEL_ERROR, DBG_PNP,  "WdfDpcCreate(CommErrorDpc) failed  [%#08lx]\n",   status);
        return status;
    }

    //
    // This dpc is fired off when the transmit immediate char
    // character is given to the hardware.  It will simply complete
    // the request.
    //

   WDF_DPC_CONFIG_INIT(&dpcConfig, SerialCompleteImmediate);

   dpcConfig.AutomaticSerialization = TRUE;

   WDF_OBJECT_ATTRIBUTES_INIT(&dpcAttributes);
   dpcAttributes.ParentObject = pDevExt->WdfDevice;

   status = WdfDpcCreate(&dpcConfig,
                                    &dpcAttributes,
                                    &pDevExt->CompleteImmediateDpc);
    if (!NT_SUCCESS(status)) {
        SerialDbgPrintEx(TRACE_LEVEL_ERROR, DBG_PNP,  "WdfDpcCreate(CompleteImmediateDpc) failed  [%#08lx]\n",   status);
        return status;
    }

    //
    // This dpc is fired off if an event occurs and there was
    // a request waiting on that event.  A dpc routine will execute
    // that completes the request.
    //
    WDF_DPC_CONFIG_INIT(&dpcConfig, SerialCompleteWait);

    dpcConfig.AutomaticSerialization = TRUE;

    WDF_OBJECT_ATTRIBUTES_INIT(&dpcAttributes);
    dpcAttributes.ParentObject = pDevExt->WdfDevice;

    status = WdfDpcCreate(&dpcConfig,
                                &dpcAttributes,
                                &pDevExt->CommWaitDpc);
    if (!NT_SUCCESS(status)) {

        SerialDbgPrintEx(TRACE_LEVEL_ERROR, DBG_PNP,  "WdfDpcCreate(CommWaitDpc) failed  [%#08lx]\n",   status);
        return status;
    }

    //
    // This dpc is fired off if the xoff counter actually runs down
    // to zero.
    //
    WDF_DPC_CONFIG_INIT(&dpcConfig, SerialCompleteXoff);

    dpcConfig.AutomaticSerialization = TRUE;

    WDF_OBJECT_ATTRIBUTES_INIT(&dpcAttributes);
    dpcAttributes.ParentObject = pDevExt->WdfDevice;

    status = WdfDpcCreate(&dpcConfig,
                                &dpcAttributes,
                                &pDevExt->XoffCountCompleteDpc);

    if (!NT_SUCCESS(status)) {
        SerialDbgPrintEx(TRACE_LEVEL_ERROR, DBG_PNP,  "WdfDpcCreate(XoffCountCompleteDpc) failed  [%#08lx]\n",   status);
        return status;
    }


    //
    // This dpc is fired off only from device level to start off
    // a timer that will queue a dpc to check if the RTS line
    // should be lowered when we are doing transmit toggling.
    //
    WDF_DPC_CONFIG_INIT(&dpcConfig, SerialStartTimerLowerRTS);

    dpcConfig.AutomaticSerialization = TRUE;

    WDF_OBJECT_ATTRIBUTES_INIT(&dpcAttributes);
    dpcAttributes.ParentObject = pDevExt->WdfDevice;

    status = WdfDpcCreate(&dpcConfig,
                                &dpcAttributes,
                                &pDevExt->StartTimerLowerRTSDpc);
    if (!NT_SUCCESS(status)) {
        SerialDbgPrintEx(TRACE_LEVEL_ERROR, DBG_PNP,  "WdfDpcCreate(StartTimerLowerRTSDpc) failed  [%#08lx]\n",   status);
        return status;
    }

    return status;
}




BOOLEAN
SerialInsertQueueDpc(IN WDFDPC PDpc)
/*++

Routine Description:

   This function must be called to queue DPC's for the serial driver.

Arguments:

   PDpc - Pointer to the Dpc object

Return Value:

   Kicks up return value from KeInsertQueueDpc()

--*/
{
    //
    // If the specified DPC object is not currently in the queue, WdfDpcEnqueue
    // queues the DPC and returns TRUE.
    //

    return WdfDpcEnqueue(PDpc);
}



BOOLEAN
SerialSetTimer(IN WDFTIMER Timer, IN LARGE_INTEGER DueTime)
/*++

Routine Description:

   This function must be called to set timers for the serial driver.

Arguments:

   Timer - pointer to timer dispatcher object

   DueTime - time at which the timer should expire


Return Value:

   Kicks up return value from KeSetTimerEx()

--*/
{
    BOOLEAN result;
    //
    // If the timer object was already in the system timer queue, WdfTimerStart returns TRUE
    //
    result = WdfTimerStart(Timer, DueTime.QuadPart);

    return result;

}


VOID
SerialDrainTimersAndDpcs(
    IN PSERIAL_DEVICE_EXTENSION PDevExt
    )
/*++

Routine Description:

   This function cancels all the timers and Dpcs and waits for them
   to run to completion if they are already fired.

Arguments:

   PDevExt - Pointer to the device extension for the device that needs to
             set a timer

Return Value:

--*/
{
    WdfTimerStop(PDevExt->ReadRequestTotalTimer, TRUE);

    WdfTimerStop(PDevExt->ReadRequestIntervalTimer, TRUE);

    WdfTimerStop(PDevExt->WriteRequestTotalTimer, TRUE);

    WdfTimerStop(PDevExt->ImmediateTotalTimer, TRUE);

    WdfTimerStop(PDevExt->XoffCountTimer, TRUE);

    WdfTimerStop(PDevExt->LowerRTSTimer, TRUE);

    WdfDpcCancel(PDevExt->CompleteWriteDpc, TRUE);

    WdfDpcCancel(PDevExt->CompleteReadDpc, TRUE);

    WdfDpcCancel(PDevExt->CommErrorDpc, TRUE);

    WdfDpcCancel(PDevExt->CompleteImmediateDpc, TRUE);

    WdfDpcCancel(PDevExt->CommWaitDpc, TRUE);

    WdfDpcCancel(PDevExt->XoffCountCompleteDpc, TRUE);

    WdfDpcCancel(PDevExt->StartTimerLowerRTSDpc, TRUE);

    return;
}



BOOLEAN
SerialCancelTimer(
    IN WDFTIMER                 Timer,
    IN PSERIAL_DEVICE_EXTENSION PDevExt
    )
/*++

Routine Description:

   This function must be called to cancel timers for the serial driver.

Arguments:

   Timer - pointer to timer dispatcher object

   PDevExt - Pointer to the device extension for the device that needs to
             set a timer

Return Value:

   True if timer was cancelled

--*/
{
    UNREFERENCED_PARAMETER(PDevExt);

    return WdfTimerStop(Timer, FALSE);
}

SERIAL_MEM_COMPARES
SerialMemCompare(
                IN PHYSICAL_ADDRESS A,
                IN ULONG SpanOfA,
                IN PHYSICAL_ADDRESS B,
                IN ULONG SpanOfB
                )
/*++

Routine Description:

    Compare two phsical address.

Arguments:

    A - One half of the comparison.

    SpanOfA - In units of bytes, the span of A.

    B - One half of the comparison.

    SpanOfB - In units of bytes, the span of B.


Return Value:

    The result of the comparison.

--*/
{
   LARGE_INTEGER a;
   LARGE_INTEGER b;

   LARGE_INTEGER lower;
   ULONG lowerSpan;
   LARGE_INTEGER higher;

   PAGED_CODE();

   a = A;
   b = B;

   if (a.QuadPart == b.QuadPart) {

      return AddressesAreEqual;

   }

   if (a.QuadPart > b.QuadPart) {

      higher = a;
      lower = b;
      lowerSpan = SpanOfB;

   } else {

      higher = b;
      lower = a;
      lowerSpan = SpanOfA;

   }

   if ((higher.QuadPart - lower.QuadPart) >= lowerSpan) {

      return AddressesAreDisjoint;

   }

   return AddressesOverlap;

}


VOID
SerialLogError(
    __in                             PDRIVER_OBJECT DriverObject,
    __in_opt                         PDEVICE_OBJECT DeviceObject,
    __in                             PHYSICAL_ADDRESS P1,
    __in                             PHYSICAL_ADDRESS P2,
    __in                             ULONG SequenceNumber,
    __in                             UCHAR MajorFunctionCode,
    __in                             UCHAR RetryCount,
    __in                             ULONG UniqueErrorValue,
    __in                             NTSTATUS FinalStatus,
    __in                             NTSTATUS SpecificIOStatus,
    __in                             ULONG LengthOfInsert1,
    __in_bcount_opt(LengthOfInsert1) PWCHAR Insert1,
    __in                             ULONG LengthOfInsert2,
    __in_bcount_opt(LengthOfInsert2) PWCHAR Insert2
    )
/*++

Routine Description:

    This routine allocates an error log entry, copies the supplied data
    to it, and requests that it be written to the error log file.

Arguments:

    DriverObject - A pointer to the driver object for the device.

    DeviceObject - A pointer to the device object associated with the
    device that had the error, early in initialization, one may not
    yet exist.

    P1,P2 - If phyical addresses for the controller ports involved
    with the error are available, put them through as dump data.

    SequenceNumber - A ulong value that is unique to an WDFREQUEST over the
    life of the request in this driver - 0 generally means an error not
    associated with an request.

    MajorFunctionCode - If there is an error associated with the request,
    this is the major function code of that request.

    RetryCount - The number of times a particular operation has been
    retried.

    UniqueErrorValue - A unique long word that identifies the particular
    call to this function.

    FinalStatus - The final status given to the request that was associated
    with this error.  If this log entry is being made during one of
    the retries this value will be STATUS_SUCCESS.

    SpecificIOStatus - The IO status for a particular error.

    LengthOfInsert1 - The length in bytes (including the terminating NULL)
                      of the first insertion string.

    Insert1 - The first insertion string.

    LengthOfInsert2 - The length in bytes (including the terminating NULL)
                      of the second insertion string.  NOTE, there must
                      be a first insertion string for their to be
                      a second insertion string.

    Insert2 - The second insertion string.

Return Value:

    None.

--*/

{
   PIO_ERROR_LOG_PACKET errorLogEntry;

   PVOID objectToUse;
   SHORT dumpToAllocate = 0;
   PUCHAR ptrToFirstInsert;
   PUCHAR ptrToSecondInsert;

   PAGED_CODE();

   if (Insert1 == NULL) {
      LengthOfInsert1 = 0;
   }

   if (Insert2 == NULL) {
      LengthOfInsert2 = 0;
   }


   if (ARGUMENT_PRESENT(DeviceObject)) {

      objectToUse = DeviceObject;

   } else {

      objectToUse = DriverObject;

   }

   if (SerialMemCompare(
                       P1,
                       (ULONG)1,
                       SerialPhysicalZero,
                       (ULONG)1
                       ) != AddressesAreEqual) {

      dumpToAllocate = (SHORT)sizeof(PHYSICAL_ADDRESS);

   }

   if (SerialMemCompare(
                       P2,
                       (ULONG)1,
                       SerialPhysicalZero,
                       (ULONG)1
                       ) != AddressesAreEqual) {

      dumpToAllocate += (SHORT)sizeof(PHYSICAL_ADDRESS);

   }

   errorLogEntry = IoAllocateErrorLogEntry(
                                          objectToUse,
                                          (UCHAR)(sizeof(IO_ERROR_LOG_PACKET) +
                                                  dumpToAllocate
                                                  + LengthOfInsert1 +
                                                  LengthOfInsert2)
                                          );

   if ( errorLogEntry != NULL ) {

      errorLogEntry->ErrorCode = SpecificIOStatus;
      errorLogEntry->SequenceNumber = SequenceNumber;
      errorLogEntry->MajorFunctionCode = MajorFunctionCode;
      errorLogEntry->RetryCount = RetryCount;
      errorLogEntry->UniqueErrorValue = UniqueErrorValue;
      errorLogEntry->FinalStatus = FinalStatus;
      errorLogEntry->DumpDataSize = dumpToAllocate;

      if (dumpToAllocate) {

         RtlCopyMemory(
                      &errorLogEntry->DumpData[0],
                      &P1,
                      sizeof(PHYSICAL_ADDRESS)
                      );

         if (dumpToAllocate > sizeof(PHYSICAL_ADDRESS)) {

            RtlCopyMemory(
                         ((PUCHAR)&errorLogEntry->DumpData[0])
                         +sizeof(PHYSICAL_ADDRESS),
                         &P2,
                         sizeof(PHYSICAL_ADDRESS)
                         );

            ptrToFirstInsert =
            ((PUCHAR)&errorLogEntry->DumpData[0])+(2*sizeof(PHYSICAL_ADDRESS));

         } else {

            ptrToFirstInsert =
            ((PUCHAR)&errorLogEntry->DumpData[0])+sizeof(PHYSICAL_ADDRESS);


         }

      } else {

         ptrToFirstInsert = (PUCHAR)&errorLogEntry->DumpData[0];

      }

      ptrToSecondInsert = ptrToFirstInsert + LengthOfInsert1;

      if (LengthOfInsert1) {

         errorLogEntry->NumberOfStrings = 1;
         errorLogEntry->StringOffset = (USHORT)(ptrToFirstInsert -
                                                (PUCHAR)errorLogEntry);
         RtlCopyMemory(
                      ptrToFirstInsert,
                      Insert1,
                      LengthOfInsert1
                      );

         if (LengthOfInsert2) {

            errorLogEntry->NumberOfStrings = 2;
            RtlCopyMemory(
                         ptrToSecondInsert,
                         Insert2,
                         LengthOfInsert2
                         );

         }

      }

      IoWriteErrorLogEntry(errorLogEntry);

   }

}

VOID
SerialMarkHardwareBroken(IN PSERIAL_DEVICE_EXTENSION PDevExt)
/*++

Routine Description:

   Marks a UART as broken.  This causes the driver stack to stop accepting
   requests and eventually be removed.

Arguments:
   PDevExt - Device extension attached to PDevObj

Return Value:

   None.

--*/
{
   PAGED_CODE();

   //
   // Write a log entry
   //

   SerialLogError(PDevExt->DriverObject, NULL, SerialPhysicalZero,
                  SerialPhysicalZero, 0, 0, 0, 88, STATUS_SUCCESS,
                  SERIAL_HARDWARE_FAILURE, PDevExt->DeviceName.Length
                  + sizeof(WCHAR), PDevExt->DeviceName.Buffer, 0, NULL);

   SerialDbgPrintEx(TRACE_LEVEL_ERROR, DBG_INIT, "Device is broken. Request a restart...\n");
   WdfDeviceSetFailed(PDevExt->WdfDevice, WdfDeviceFailedAttemptRestart);
}

NTSTATUS
SerialGetDivisorFromBaud(
                        IN ULONG ClockRate,
                        IN LONG DesiredBaud,
                        OUT PSHORT AppropriateDivisor
                        )

/*++

Routine Description:

    This routine will determine a divisor based on an unvalidated
    baud rate.

Arguments:

    ClockRate - The clock input to the controller.

    DesiredBaud - The baud rate for whose divisor we seek.

    AppropriateDivisor - Given that the DesiredBaud is valid, the
    LONG pointed to by this parameter will be set to the appropriate
    value.  NOTE: The long is undefined if the DesiredBaud is not
    supported.

Return Value:

    This function will return STATUS_SUCCESS if the baud is supported.
    If the value is not supported it will return a status such that
    NT_ERROR(Status) == FALSE.

--*/

{

   NTSTATUS status = STATUS_SUCCESS;
   SHORT calculatedDivisor;
   ULONG denominator;
   ULONG remainder;

   //
   // Allow up to a 1 percent error
   //

   ULONG maxRemain18 = 18432;
   ULONG maxRemain30 = 30720;
   ULONG maxRemain42 = 42336;
   ULONG maxRemain80 = 80000;
   ULONG maxRemain;



   //
   // Reject any non-positive bauds.
   //

   denominator = DesiredBaud*(ULONG)16;

   if (DesiredBaud <= 0) {

      *AppropriateDivisor = -1;

   } else if ((LONG)denominator < DesiredBaud) {

      //
      // If the desired baud was so huge that it cause the denominator
      // calculation to wrap, don't support it.
      //

      *AppropriateDivisor = -1;

   } else {

      if (ClockRate == 1843200) {
         maxRemain = maxRemain18;
      } else if (ClockRate == 3072000) {
         maxRemain = maxRemain30;
      } else if (ClockRate == 4233600) {
         maxRemain = maxRemain42;
      } else {
         maxRemain = maxRemain80;
      }

      calculatedDivisor = (SHORT)(ClockRate / denominator);
      remainder = ClockRate % denominator;

      //
      // Round up.
      //

      if (((remainder*2) > ClockRate) && (DesiredBaud != 110)) {

         calculatedDivisor++;
      }


      //
      // Only let the remainder calculations effect us if
      // the baud rate is > 9600.
      //

      if (DesiredBaud >= 9600) {

         //
         // If the remainder is less than the maximum remainder (wrt
         // the ClockRate) or the remainder + the maximum remainder is
         // greater than or equal to the ClockRate then assume that the
         // baud is ok.
         //

         if ((remainder >= maxRemain) && ((remainder+maxRemain) < ClockRate)) {
            calculatedDivisor = -1;
         }

      }

      //
      // Don't support a baud that causes the denominator to
      // be larger than the clock.
      //

      if (denominator > ClockRate) {

         calculatedDivisor = -1;

      }

      //
      // Ok, Now do some special casing so that things can actually continue
      // working on all platforms.
      //

      if (ClockRate == 1843200) {

         if (DesiredBaud == 56000) {
            calculatedDivisor = 2;
         }

      } else if (ClockRate == 3072000) {

         if (DesiredBaud == 14400) {
            calculatedDivisor = 13;
         }

      } else if (ClockRate == 4233600) {

         if (DesiredBaud == 9600) {
            calculatedDivisor = 28;
         } else if (DesiredBaud == 14400) {
            calculatedDivisor = 18;
         } else if (DesiredBaud == 19200) {
            calculatedDivisor = 14;
         } else if (DesiredBaud == 38400) {
            calculatedDivisor = 7;
         } else if (DesiredBaud == 56000) {
            calculatedDivisor = 5;
         }

      } else if (ClockRate == 8000000) {

         if (DesiredBaud == 14400) {
            calculatedDivisor = 35;
         } else if (DesiredBaud == 56000) {
            calculatedDivisor = 9;
         }

      }

      *AppropriateDivisor = calculatedDivisor;

   }


   if (*AppropriateDivisor == -1) {

      status = STATUS_INVALID_PARAMETER;

   }

   return status;

}


BOOLEAN
IsQueueEmpty(
    IN WDFQUEUE Queue
    )
{
    WDF_IO_QUEUE_STATE queueStatus;

    queueStatus = WdfIoQueueGetState( Queue, NULL, NULL );

    return (WDF_IO_QUEUE_IDLE(queueStatus)) ? TRUE : FALSE;
}

VOID
SerialSetCancelRoutine(
    IN WDFREQUEST Request,
    IN PFN_WDF_REQUEST_CANCEL CancelRoutine)
{
    PREQUEST_CONTEXT reqContext = SerialGetRequestContext(Request);

    SerialDbgPrintEx(TRACE_LEVEL_INFORMATION, DBG_IOCTLS,
                        "-->SerialSetCancelRoutine %p \n",  Request);

    WdfRequestMarkCancelable(Request, CancelRoutine);
    SERIAL_SET_REFERENCE(reqContext, SERIAL_REF_CANCEL);
    reqContext->CancelRoutine = CancelRoutine;

    SerialDbgPrintEx(TRACE_LEVEL_INFORMATION, DBG_IOCTLS,
                        "<-- SerialSetCancelRoutine \n");

    return;
}

NTSTATUS
SerialClearCancelRoutine(
    IN WDFREQUEST Request,
    IN BOOLEAN    ClearReference
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    PREQUEST_CONTEXT reqContext = SerialGetRequestContext(Request);

    SerialDbgPrintEx(TRACE_LEVEL_INFORMATION, DBG_IOCTLS,
                     "-->SerialClearCancelRoutine %p %x\n",
                     Request, ClearReference);

    if(SERIAL_TEST_REFERENCE(reqContext,  SERIAL_REF_CANCEL))
    {
        status = WdfRequestUnmarkCancelable(Request);
        if (NT_SUCCESS(status)) {

            reqContext->CancelRoutine = NULL;
            if(ClearReference) {

               SERIAL_CLEAR_REFERENCE( reqContext,  SERIAL_REF_CANCEL );

              }
        } else {
             ASSERT(status == STATUS_CANCELLED);
        }
    }

    SerialDbgPrintEx(TRACE_LEVEL_INFORMATION, DBG_IOCTLS,
                        "-->SerialClearCancelRoutine %p\n",  Request);

    return status;
}


VOID
SerialCompleteRequest(
    IN WDFREQUEST   Request,
    IN NTSTATUS     Status,
    IN ULONG_PTR    Info
    )
{
    PREQUEST_CONTEXT reqContext;

    reqContext = SerialGetRequestContext(Request);

    ASSERT(reqContext->RefCount == 0);

    SerialDbgPrintEx(TRACE_LEVEL_VERBOSE, DBG_PNP,
                     "Complete Request: %p %X 0x%I64x\n",
                     (Request), (Status), (Info));

    WdfRequestCompleteWithInformation((Request), (Status), (Info));

}


BOOLEAN  set_rx_trigger(PVOID Context)
{
	ULONG savereg;
	PUCHAR port;
	ULONG val;
	PSERIAL_DEVICE_EXTENSION extension;
	extension = (PSERIAL_DEVICE_EXTENSION)Context;
	port = extension->Controller;
	val = extension->RxTrigger;
	
	
	//DbgPrint("set_rx_trigger port:%x val:%x\n",port,val);
	
	if(port==0)return FALSE; 				
	if(extension->type850or950==1)
	{
		//950 routine
		UCHAR temp;
		//DbgPrint("950\n");
		savereg = READ_PORT_UCHAR(port+3);
		WRITE_PORT_UCHAR(port+3,0xbf);
		WRITE_PORT_UCHAR(port+2,0x10);//turn on enhanced mode
		
		WRITE_PORT_UCHAR(port+3,0);
		WRITE_PORT_UCHAR(port+7,0);
		if(extension->Auto485==1) temp = 0x10;
		else temp = 0;
		WRITE_PORT_UCHAR(port+5,(UCHAR)(temp|0x20));//enable triggers
		WRITE_PORT_UCHAR(port+3,0xbf);
		WRITE_PORT_UCHAR(port+2,0x00);//clear the EFR bit (Latch extendeds);
		WRITE_PORT_UCHAR(port+3,(UCHAR)savereg);
		
		
		savereg = READ_PORT_UCHAR(port+3);
		WRITE_PORT_UCHAR(port+3,0);
		WRITE_PORT_UCHAR(port+7,5);
		WRITE_PORT_UCHAR(port+5,(UCHAR)(val&0x7f));
		if(extension->PortType==0x17)
		{
			//for our custom 4k buffer, extended trigger values
			WRITE_PORT_UCHAR(port+7,0x15);
			WRITE_PORT_UCHAR(port+5,(UCHAR)((val>>7)&0x3f));
		}
		WRITE_PORT_UCHAR(port+3,(UCHAR)savereg);
		
	}
	else
	{
		//850 routine
		//DbgPrint("850\n");
		savereg = READ_PORT_UCHAR(port+3);
		//SimplDrvKdPrint(("user rx trig table D\n"));
		//myprintf("Set Startech to TX trigger table C mode\r\n");
		WRITE_PORT_UCHAR(port+3,0xBF);//set to get to Extended regs
		WRITE_PORT_UCHAR(port+2,0x10);//set the EFR bit (enable extendeds);
		WRITE_PORT_UCHAR(port+1,(UCHAR)((READ_PORT_UCHAR(port+1)&(UCHAR)0x4f)|(UCHAR)0x30));//set to tx tab c (not killing other modes)
		//DbgPrint("afterwrite:%x",READ_PORT_UCHAR(port+1)&0xff);
		WRITE_PORT_UCHAR(port,(UCHAR)val);//get user trig setting and set here (instead of 100)
		WRITE_PORT_UCHAR(port+2,0x00);//clear the EFR bit (Latch extendeds);
		WRITE_PORT_UCHAR(port+3,(UCHAR)savereg);//set to get to Normal regs
	}
	return TRUE;
}


BOOLEAN  set_tx_trigger(PVOID Context)
{
	ULONG savereg;
	PUCHAR port;
	ULONG val;
	PSERIAL_DEVICE_EXTENSION extension;
	extension = (PSERIAL_DEVICE_EXTENSION)Context;
	port = extension->Controller;
	val = extension->TxTrigger;
	
	//DbgPrint("set_tx_trigger port:%x val:%x\n",port,val);
	
	if(port==0)return FALSE; 				
	if(extension->type850or950==1)
	{
		UCHAR temp;
		//DbgPrint("950\n");
		savereg = READ_PORT_UCHAR(port+3);
		WRITE_PORT_UCHAR(port+3,0xbf);
		WRITE_PORT_UCHAR(port+2,0x10);//turn on enhanced mode
		
		WRITE_PORT_UCHAR(port+3,0);
		WRITE_PORT_UCHAR(port+7,0);
		if(extension->Auto485==1) temp = 0x10;
		else temp = 0;
		WRITE_PORT_UCHAR(port+5,(UCHAR)(temp|0x20));//enable triggers
		WRITE_PORT_UCHAR(port+3,0xbf);
		WRITE_PORT_UCHAR(port+2,0x00);//clear the EFR bit (Latch extendeds);
		WRITE_PORT_UCHAR(port+3,(UCHAR)savereg);
		
		savereg = READ_PORT_UCHAR(port+3);
		WRITE_PORT_UCHAR(port+3,0);
		WRITE_PORT_UCHAR(port+7,4);
		WRITE_PORT_UCHAR(port+5,(UCHAR)(val&0x7f));
		
		if(extension->PortType==0x17)
		{
			//only present on our "950" ip
			
			//for our custom 4k buffer, extended trigger values
			WRITE_PORT_UCHAR(port+7,0x14);
			WRITE_PORT_UCHAR(port+5,(UCHAR)((val>>7)&0x3f));
		}	
		WRITE_PORT_UCHAR(port+3,(UCHAR)savereg);
	}
	else
	{
		//DbgPrint("850\n");
		savereg = READ_PORT_UCHAR(port+3);
		//SimplDrvKdPrint(("User tx trig tab D\n"));
		//myprintf("Set Startech to TX trigger table C mode\r\n");
		WRITE_PORT_UCHAR(port+3,0xBF);//set to get to Extended regs
		WRITE_PORT_UCHAR(port+2,0x10);//set the EFR bit (enable extendeds);
		WRITE_PORT_UCHAR(port+1,(UCHAR)((READ_PORT_UCHAR(port+1)&(UCHAR)0xcf)|(UCHAR)0xb0));//set to tx tab c (not killing other modes)
		//DbgPrint("afterwrite:%x",READ_PORT_UCHAR(port+1)&0xff);
		WRITE_PORT_UCHAR(port,(UCHAR)val);//get user trig setting and set here (instead of 100)
		WRITE_PORT_UCHAR(port+2,0x00);//clear the EFR bit (Latch extendeds);
		WRITE_PORT_UCHAR(port+3,(UCHAR)savereg);//set to get to Normal regs
	}
	return TRUE;
}


BOOLEAN  set_ext_count(PVOID Context)
{
	ULONG savereg;
	PUCHAR port;
	ULONG val;
	PSERIAL_DEVICE_EXTENSION extension;
	extension = (PSERIAL_DEVICE_EXTENSION)Context;
	port = extension->Controller;
	if(extension->ExtCount>0) val = extension->ExtCount-1;
	else val=0;
	
	
	//DbgPrint("set_rx_trigger port:%x val:%x\n",port,val);
	
	if(port==0)return FALSE; 				
	if(extension->type850or950==1)
	{
		//950 routine
		UCHAR temp;
		//DbgPrint("950\n");
		savereg = READ_PORT_UCHAR(port+3);
		WRITE_PORT_UCHAR(port+3,0xbf);
		WRITE_PORT_UCHAR(port+2,0x10);//turn on enhanced mode
		WRITE_PORT_UCHAR(port+3,0);
		
		if(val==0)
		{
			WRITE_PORT_UCHAR(port+7,0x16);
			WRITE_PORT_UCHAR(port+5,(UCHAR)(0x00));
			
			WRITE_PORT_UCHAR(port+7,0x17);
			WRITE_PORT_UCHAR(port+5,(UCHAR)(0x00));//force enable to 0 to turn off external transmit mode
		}
		else
		{
			WRITE_PORT_UCHAR(port+7,0x16);
			WRITE_PORT_UCHAR(port+5,(UCHAR)(val&0xff));//set external transmit count
			
			WRITE_PORT_UCHAR(port+7,0x17);
			WRITE_PORT_UCHAR(port+5,(UCHAR)(((val>>8)&0x1f)|0x80));//set external transmit count (upper portion) + enable bit
		}
		WRITE_PORT_UCHAR(port+3,0xbf);
		WRITE_PORT_UCHAR(port+2,0x00);//turn off enhanced mode
		WRITE_PORT_UCHAR(port+3,(UCHAR)savereg);
		
	}
	else
	{
		//850 routine
		//DbgPrint("850\n");
		//there is no 850 portion of this...so endgame? eh just ignore
	}
	return TRUE;
}


BOOLEAN  enable_auto_485(PVOID Context)
{
	ULONG savereg;
	PUCHAR port;
	ULONG val;
	UCHAR temp;
	PSERIAL_DEVICE_EXTENSION extension;
	extension = (PSERIAL_DEVICE_EXTENSION)Context;
	port = extension->Controller;
	val = extension->Auto485;
	
	//DbgPrint("enable_auto_485 port:%x val:%x\n",port,val);
	
	if (port == 0)
		return FALSE;

	if (extension->type850or950 == 1) { // 950
		savereg = READ_PORT_UCHAR(port + 3);

		WRITE_PORT_UCHAR(port + 3, 0xBF);
		WRITE_PORT_UCHAR(port + 2, 0x10); // Enable enhanced mode
		
		WRITE_PORT_UCHAR(port + 3, 0);
		WRITE_PORT_UCHAR(port + 7, 0);

		temp = 0x20; // Always assume triggers are active

		if (val == 0)	
			WRITE_PORT_UCHAR(port + 5, (UCHAR)(temp & 0xEF)); // Disable 485
		if (val == 1)  
			WRITE_PORT_UCHAR(port + 5, (UCHAR)(temp | 0x10)); // Enable 485

		WRITE_PORT_UCHAR(port + 3, 0xBF);
		WRITE_PORT_UCHAR(port + 2, 0x00); // Clear the EFR bit (Latch extendeds);
		WRITE_PORT_UCHAR(port + 3, (UCHAR)savereg);
	}
	else { // 850
		savereg = READ_PORT_UCHAR(port + 3);

		WRITE_PORT_UCHAR(port + 3, 0xBF); // Set to get to Extended regs
		WRITE_PORT_UCHAR(port + 2, 0x10); // Set the EFR bit (enable extendeds);

		if (val == 1) 
			WRITE_PORT_UCHAR(port + 1,(UCHAR)((READ_PORT_UCHAR(port + 1) & (UCHAR)0xf7) | (UCHAR)0x8)); // Enable auto 485 mode
		else 
			WRITE_PORT_UCHAR(port + 1,(UCHAR)((READ_PORT_UCHAR(port + 1) & (UCHAR)0xf7))); // Disable auto 485 mode

		//DbgPrint("afterwrite:%x",READ_PORT_UCHAR(port+1)&0xff);

		WRITE_PORT_UCHAR(port + 2, 0x00); // Clear the EFR bit (Latch extendeds);
		WRITE_PORT_UCHAR(port + 3, (UCHAR)savereg);
	}

	return TRUE;
}

BOOLEAN  force_transmitter_on(PVOID Context)
{
	ULONG savereg;
	PUCHAR port;
	ULONG val;
	PSERIAL_DEVICE_EXTENSION extension;

	extension = (PSERIAL_DEVICE_EXTENSION)Context;
	port = extension->Controller;
	if(port==0)return FALSE; 				
	val = READ_PORT_UCHAR(port+4);//get MCR
	extension->savedmcr = val;
	val = val | 0xf; //(force RTS,OUT1, OUT2, DTR)
	
	WRITE_PORT_UCHAR(port+4,(UCHAR)(val));
	
	//this doesn't work!
	val = extension->ConfigReg | 0x08;//force RTS control, and RTS on
	setconfigreg(extension->configreg_add,extension->childportnumber,val,extension);
	
	return TRUE;
}

BOOLEAN  force_transmitter_off(PVOID Context)
{
	ULONG savereg;
	PUCHAR port;
	ULONG orig_reg;
	ULONG val;
	PSERIAL_DEVICE_EXTENSION extension;
	extension = (PSERIAL_DEVICE_EXTENSION)Context;
	port = extension->Controller;
	if(port==0)return FALSE; 				
	
	//!This doesn't work!
	val = extension->ConfigReg; 
	setconfigreg(extension->configreg_add,extension->childportnumber,val,extension);
	
	WRITE_PORT_UCHAR(port+4,(UCHAR)(extension->savedmcr));
	return TRUE;
}


BOOLEAN  set4x(PVOID Context)
{
	ULONG savereg;
	PUCHAR port;
	ULONG val;
	PSERIAL_DEVICE_EXTENSION extension;
	extension = (PSERIAL_DEVICE_EXTENSION)Context;
	port = extension->Controller;
	val = extension->Clock4X;
	
	
	//DbgPrint("set4x port:%x val:%x\n",port,val);
	
	if(port==0)return FALSE; 				
	if(extension->type850or950==1)
	{
		//DbgPrint("950\n");
		savereg = READ_PORT_UCHAR(port+3);
		WRITE_PORT_UCHAR(port+3,0xBF);//set to get to Extended regs
		WRITE_PORT_UCHAR(port+2,0x10);//set the EFR bit (enable extendeds);
		WRITE_PORT_UCHAR(port+3,(UCHAR)savereg);
		if(val==1) WRITE_PORT_UCHAR(port+4,(UCHAR)(READ_PORT_UCHAR(port+4)|0x80));//if true set to divide by 4
		else WRITE_PORT_UCHAR(port+4,(UCHAR)(READ_PORT_UCHAR(port+4)&0x7F));//else set to divide by 1
		WRITE_PORT_UCHAR(port+3,0xBF);//set to get to Extended regs
		WRITE_PORT_UCHAR(port+2,0x00);//set the EFR bit (enable extendeds);
		WRITE_PORT_UCHAR(port+3,(UCHAR)savereg);
	}
	else
	{
		//DbgPrint("850\n");
		savereg = READ_PORT_UCHAR(port+3);
		WRITE_PORT_UCHAR(port+3,0xBF);//set to get to Extended regs
		WRITE_PORT_UCHAR(port+2,0x10);//set the EFR bit (enable extendeds);
		WRITE_PORT_UCHAR(port+3,(UCHAR)savereg);
		if(val==1) WRITE_PORT_UCHAR(port+4,(UCHAR)(READ_PORT_UCHAR(port+4)|0x80));//if true set to divide by 4
		else WRITE_PORT_UCHAR(port+4,(UCHAR)(READ_PORT_UCHAR(port+4)&0x7F));//else set to divide by 1
		WRITE_PORT_UCHAR(port+3,0xBF);//set to get to Extended regs
		WRITE_PORT_UCHAR(port+2,0x00);//set the EFR bit (enable extendeds);
		WRITE_PORT_UCHAR(port+3,(UCHAR)savereg);
	}
	return TRUE;
}

void setconfigreg(PUCHAR port,ULONG childid,ULONG val,PSERIAL_DEVICE_EXTENSION extension)
{
	ULONG orig_reg;
	ULONG mask;
	ULONG reg;
	KIRQL oldIrql;
	//rework this for FSCC
	//DbgPrint("SetConfig: configreg:%x child:%d value:%x\n",port,childid,val);
	
	if(port==0)return; 				
	
	KeAcquireSpinLock(extension->pboardlock, &oldIrql);
	
	
	
	
	orig_reg = READ_PORT_ULONG((PULONG)port);
	
	//DbgPrint("Original ConfigReg:%8.8x\n",orig_reg);
	
	/*
	if((configreg&1)==1) Params->PortSettings.CTSDisable=1; !!doesn't exist
	else Params->PortSettings.CTSDisable=0; 
	
	  if((configreg&2)==2) Params->PortSettings.Enable485=1;
	  else Params->PortSettings.Enable485=0;
	  
		if((configreg&4)==4) Params->PortSettings.RxEchoCancel=1;
		else Params->PortSettings.RxEchoCancel=0;
		
		  if((configreg&8)==8) Params->PortSettings.ControlSource485=1; !!doesn't exist, only dtr
		  else Params->PortSettings.ControlSource485=0;
		  
			
			  assign EN485_CTL   = my_io_reg3[18];
			  assign EN485TT_CTL = my_io_reg3[17];
			  assign RXECHO_CTL	 = my_io_reg3[16];
			  
				
				  assign EN485_CTLB   = my_io_reg3[22];
				  assign EN485TT_CTLB = my_io_reg3[21];
				  assign RXECHO_CTLB	 = my_io_reg3[20];
				  
					
	*/
	
	if(childid==0) 
	{
		if((orig_reg&0x01000000)!=0)
		{
			//only set if in async mode
			orig_reg = orig_reg & 0xfef8ffff;
			if((val&2)!=0)  orig_reg |= 0x00040000;
			if((val&4)!=0)  orig_reg |= 0x00010000;
			if((val&0x80000000)!=0) orig_reg |= 0x01000000;
		}
		else
		{
			//allow switch into async mode here!
			orig_reg = orig_reg & 0xfeffffff;
			if((val&0x80000000)!=0) orig_reg |= 0x01000000;
		}
	}
	else
	{
		if((orig_reg&0x02000000)!=0)
		{
			//only set if in async mode
			orig_reg = orig_reg & 0xfd8fffff;
			if((val&2)!=0)  orig_reg |= 0x00400000;
			if((val&4)!=0)  orig_reg |= 0x00100000;
			if((val&0x80000000)!=0) orig_reg |= 0x02000000;
		}
		else
		{
			//allow switch into async mode here!
			orig_reg = orig_reg & 0xfdffffff;
			if((val&0x80000000)!=0) orig_reg |= 0x02000000;
		}
	}
	
	WRITE_PORT_ULONG((PULONG)port,orig_reg);
	//DbgPrint("New      ConfigReg:%8.8x\n",orig_reg);
	
	
	KeReleaseSpinLock(extension->pboardlock, oldIrql);
	
	
	
}




//rework for fscc clock generator...
#if 0
BOOLEAN new_clock_stuff(IN PSERIAL_DEVICE_EXTENSION deviceExtension,IN ULONG *freq)
{
	KFLOATING_SAVE  FloatSave;
	NTSTATUS ret;
	int t;
	unsigned long desiredppm;
	unsigned char progwords[19];
	
	desiredppm = deviceExtension->ppm;
	
	if(ret=KeSaveFloatingPointState(&FloatSave)==STATUS_SUCCESS)
	{
		struct ResultStruct solutiona;	//final results for ResultStruct data calculations
		struct IcpRsStruct solutionb;	//final results for IcpRsStruct data calculations
		memset(&solutiona,0,sizeof(struct ResultStruct));
		memset(&solutionb,0,sizeof(struct IcpRsStruct));
		
		t=GetICS30703Data(freq[0], desiredppm, &solutiona, &solutionb, &progwords[0]);
		switch(t)
		{
		case 0: 
			//DbgPrint("ICS30703: GetICS30703Data returned successfully.\n");
			break;
		case 1:
			DbgPrint("ICS30703: Rs case error\n");
			goto drop;
			break;
		case 2:
			DbgPrint("ICS30703: no solutions found, try increasing ppm\n");
			goto drop;
			break;
		case 3:
			DbgPrint("ICS30703: Table 1: Input Divider is out of rante.\n");
			goto drop;
			break;
		case 4:
			DbgPrint("ICS30703: Table 2: VCODivider is out of range.\n");
			goto drop;
			break;
		case 5:
			DbgPrint("ICS30703: Table 4: LoopFilterResistor is incorrect.\n");
			goto drop;
			break;
		case 6:
			DbgPrint("ICS30703: Table 3: Charge Pump Current is incorrect.\n");
			goto drop;
			break;
		case 7:
			DbgPrint("ICS30703: Table 5: OutputDividerOut1 is out of range.\n");
			goto drop;
			break;
		default:
			DbgPrint("ICS30703: Unknown error number.\n");
			goto drop;
		}
		
		/*
		DbgPrint("The One:\n");
		DbgPrint(" RD = %4i, ",solutiona.refDiv);
		DbgPrint("VD = %4i, ",solutiona.VCO_Div);
		DbgPrint("OD = %4i, ",solutiona.outDiv);
		DbgPrint("freq_MHz = %12.3f, ",solutiona.freq);
		DbgPrint(" error_PPM= %4.3f\n",solutiona.errorPPM);
		
		  DbgPrint(" Rs=%5d, ",solutionb.Rs);
		  DbgPrint("Icp=%6.2f, ",(solutionb.icp)*10e5);
		  DbgPrint("pdf=%12.2f, ",solutionb.pdf);
		  DbgPrint("nbw=%15.3f, ",solutionb.nbw);
		  DbgPrint("ratio=%5.4f, ",solutionb.ratio);
		  DbgPrint("df=%6.3f\n\n",solutionb.df);
		*/
		
		/*		DbgPrint("ICS30703: Programming word is: \n  0x");
		for(t=20;t>0;t--)
		{
		DbgPrint("%2.2X ", progwords[t-1]);
		}
		*/		
				
		if(1)
		{
			PDEVICE_OBJECT  pdo;
			PPDO_EXTENSION  pdx;
			ULONG ver;
			
			pdo = deviceExtension->Pdo;
			pdx = (PPDO_EXTENSION)pdo->DeviceExtension;
			
			
			//special casing for FSCC(board rev2.2) vs SuperFSCC(board rev 3.1)
			//get VSTR
			ver = READ_PORT_ULONG((PULONG)((PUCHAR)pdx->amccbase+0x4c));
			
			
			if((ver&0xffff0000)==0x00140000) 
			{
				if(((ver>>8)&0x00000ff) >= 2)
				{
					//override CLK/XTAL bit and force XTAL
					//DbgPrint("Super >=2\n");
					progwords[15]=progwords[15]|0x04;//new rev
				}
				else
				{
					//DbgPrint("Super <2\n");
					//prototypes need force CLK
					progwords[15]=progwords[15]&0xfb;
				}
			}
			else if((ver&0xffff0000)==0x000f0000) 
			{
				if(((ver>>8)&0x000000ff) <= 6)
				{
					//DbgPrint("Regular <=6\n");
					//firmware rev less than 6 indicates force CLK
					progwords[15]=progwords[15]&0xfb;
					//note if VSTR reads back all 0's (original firmware) then
					//we end up here, which should be the correct path.
				}
				else
				{
					//DbgPrint("Regular >6\n");
					//firmware rev greater than 6 should be "fixed" and require force XTAL
					progwords[15]=progwords[15]|0x04;
				}
			}
			else if((ver&0xffff0000)==0x00160000)
			{
				//DbgPrint("RS232 V0 (any)\n");
				//any FSCC-232 will be "new" and force xtal
				progwords[15]=progwords[15]|0x04;
			}
			else
			{
				//unknown board?!?
				DbgPrint("Unknown FSCC board! %8.8x\n",ver);
				//default it to xtal!
				progwords[15]=progwords[15]|0x04;
			}
			//DbgPrint("ver:%lx, clk15:%x\n",ver,progwords[15]);
#ifdef FSCC_GREEN_FW907
	progwords[15]=progwords[15]&0xfb;
#endif
		}
		
		if(deviceExtension->childportnumber==0)
		{
			//wait for next proto!
			t=SetICS30703Bits((PULONG)deviceExtension->configreg_add,&progwords[0],deviceExtension);
			//	t=0;
		}
		else
		{
			t=SetICS30703Bits2((PULONG)deviceExtension->configreg_add,&progwords[0],deviceExtension);
		}
		switch(t)
		{
		case 0: 
			//DbgPrint("ICS30703: SetICS30703Bits: Returned successfully.\n");
			break;
		default:
			DbgPrint("ICS30703: SetICS30703Bits: Unknown error number: %d\n",t);
			goto drop;
		}
		KeRestoreFloatingPointState(&FloatSave);
		return TRUE;
drop:
		KeRestoreFloatingPointState(&FloatSave);
		return FALSE;
	}
	else 
	{
		if(ret==STATUS_ILLEGAL_FLOAT_CONTEXT) DbgPrint("FP:bad context\n");
		if(ret==STATUS_INSUFFICIENT_RESOURCES) DbgPrint("FP:no resources\n");
		return FALSE;
	}
	return TRUE;
}

int GetICS30703Data(unsigned long desired, unsigned long ppm, struct ResultStruct *theOne, struct IcpRsStruct *theOther, unsigned char *progdata)
{
//	double inputfreq=18432000.0;
	double inputfreq=24000000.0;

	ULONG od=0;	//Output Divider
	unsigned r=0;
	unsigned v=0;
	unsigned V_Divstart=0;
	double freq=0;
	ULONG bestFreq=0;
	ULONG check=0;
	unsigned maxR;
	unsigned minR;
	unsigned max_V=2055;
	unsigned min_V=12;
	double allowable_error;
	double freq_err;
	struct ResultStruct Results;
	ULONG i,j;
	struct IcpRsStruct IRStruct;
	unsigned count;
	ULONG Rs;
	double optimal_ratio=15.0;
	double optimal_df=0.7;
	double best_ratio=0;
	double best_df=0;
	double rule1, rule2;
	int tempint;

	int InputDivider=0;
	int VCODivider=0;
	ULONG ChargePumpCurrent=0;
	ULONG LoopFilterResistor=0;
	ULONG OutputDividerOut1=0;
	ULONG OutputDividerOut2=0;
	ULONG OutputDividerOut3=0;
	ULONG temp=0;
	unsigned long requestedppm;

	requestedppm=ppm;

	if( inputfreq == 18432000.0) 
	{
		maxR = 921;
		minR = 1;
	}
	else if( inputfreq == 24000000.0) 
	{
		maxR = 1200;
		minR = 1;
	}

	ppm=0;
increaseppm:
//	DbgPrint("ICS30703: ppm = %d\n",ppm);
	allowable_error  = ppm * desired/1e6; // * 1e6

	for( r = minR; r <= maxR; r++ )
	{
		rule2 = inputfreq /(double)r;
		if ( (rule2 < 20000.0) || (rule2 > 100000000.0) )
		{
//			DbgPrint("Rule2(r=%d): 20,000<%f<100000000\n",r,rule2);
			continue;	//next r
		}

		od=8232;
		while(od > 1)
		{
			//set starting VCO setting with output freq just below target
			V_Divstart = (int) (((desired - (allowable_error) ) * r * od) / (inputfreq));

			//check if starting VCO setting too low
			if (V_Divstart < min_V)
				V_Divstart = min_V;
			
			//check if starting VCO setting too high
			else if (V_Divstart > max_V)
				V_Divstart = max_V;

			/** Loop thru VCO divide settings**/
			//Loop through all VCO divider ratios
			for( v = V_Divstart; v <= max_V; v++ ) //Check all Vco divider settings
			{
				rule1 = (inputfreq * ((double)v / (double)r) );

						if(od==2)
				{
					if( (rule1 < 90000000.0) || (rule1 > 540000000.0)  )
					{
						continue;	//next VCO_Div
					}
				}
				else if(od==3)
				{
					if( (rule1 < 90000000.0) || (rule1 > 720000000.0)  )
					{
						continue;	//next VCO_Div
					}
				}
				else if( (od>=38) && (od<=1029) )
				{
					if( (rule1 < 90000000.0) || (rule1 > 570000000.0)  )
					{
						continue;	//next VCO_Div
					}
				}
				else
				{
				if( (rule1 < 90000000.0) || (rule1 > 730000000.0)  )
				{
//					printf("Rule1: 90MHz<%f<730MHz\n",rule1);
					continue;	//next VCO_Div
					}
				}

				freq = (inputfreq * ((double)v / ((double)r * (double)od)));
				freq_err	= fabs(freq - desired) ; //Hz

				if ((freq_err) > allowable_error)
				{
					continue; //next VCO_Div
				}
				else if((freq_err) <= allowable_error)
				{
					count=0;
					for(i=0;i<4;i++)
					{
						switch(i)
						{
						case 0:
							Rs = 64000;
							break;
						case 1:
							Rs = 52000;
							break;
						case 2:
							Rs = 16000;
							break;
						case 3:
							Rs = 4000;
							break;
						default:
							return 1;
						}

						for(j=0;j<20;j++)
						{
							IRStruct.Rs=Rs;
							switch(j)
							{
							case 0:
								IRStruct.icp=1.25e-6;
								IRStruct.icpnum=125;
								break;
							case 1:
								IRStruct.icp=2.5e-6;
								IRStruct.icpnum=250;
								break;
							case 2:
								IRStruct.icp=3.75e-6;
								IRStruct.icpnum=375;
								break;
							case 3:
								IRStruct.icp=5.0e-6;
								IRStruct.icpnum=500;
								break;
							case 4:
								IRStruct.icp=6.25e-6;
								IRStruct.icpnum=625;
								break;
							case 5:
								IRStruct.icp=7.5e-6;
								IRStruct.icpnum=750;
								break;
							case 6:
								IRStruct.icp=8.75e-6;
								IRStruct.icpnum=875;
								break;
							case 7:
								IRStruct.icp=10.0e-6;
								IRStruct.icpnum=1000;
								break;
							case 8:
								IRStruct.icp=11.25e-6;
								IRStruct.icpnum=1125;
								break;
							case 9:
								IRStruct.icp=12.5e-6;
								IRStruct.icpnum=1250;
								break;
							case 10:
								IRStruct.icp=15.0e-6;
								IRStruct.icpnum=1500;
								break;
							case 11:
								IRStruct.icp=17.5e-6;
								IRStruct.icpnum=1750;
								break;
							case 12:
								IRStruct.icp=18.75e-6;
								IRStruct.icpnum=1875;
								break;
							case 13:
								IRStruct.icp=20.0e-6;
								IRStruct.icpnum=2000;
								break;
							case 14:
								IRStruct.icp=22.5e-6;
								IRStruct.icpnum=2250;
								break;
							case 15:
								IRStruct.icp=25.0e-6;
								IRStruct.icpnum=2500;
								break;
							case 16:
								IRStruct.icp=26.25e-6;
								IRStruct.icpnum=2625;
								break;
							case 17:
								IRStruct.icp=30.0e-6;
								IRStruct.icpnum=3000;
								break;
							case 18:
								IRStruct.icp=35.0e-6;
								IRStruct.icpnum=3500;
								break;
							case 19:
								IRStruct.icp=40.0e-6;
								IRStruct.icpnum=4000;
								break;
							default:
								DbgPrint("ICS30703: switch(j:icp) - You shouldn't get here! %d\n",j);
							}//end switch(j)
//							printf("Rs=%5d ",IRStruct.Rs);
//							printf("Icp=%2.2f ",IRStruct.icp*10e5);

							IRStruct.pdf = (inputfreq / (double)r) ;
//							printf("pdf=%12.2f ",IRStruct.pdf);

							IRStruct.nbw = ( ((double)IRStruct.Rs * IRStruct.icp * 310.0e6) / (2.0 * 3.14159 * (double)v) );
//							printf("nbw=%15.3f ",IRStruct.nbw);

							IRStruct.ratio = (IRStruct.pdf/IRStruct.nbw);

							tempint = (int)(IRStruct.ratio*10.0);
							if((IRStruct.ratio*10.0)-tempint>=0.0) tempint++;
							IRStruct.ratio = (double)tempint/10.0;

//							IRStruct.ratio = ceil(IRStruct.ratio*10.0);	//these two statements make the
//							IRStruct.ratio = IRStruct.ratio/10.0;		//ratio a little nicer to compare

//							printf("ratio=%2.4f ",IRStruct.ratio);

							IRStruct.df = ( ((double)IRStruct.Rs / 2) * (sqrt( ((IRStruct.icp * 0.093) / (double)v))) );
//							printf("ndf=%12.3f\n",IRStruct.df);

							count++;
							if( (IRStruct.ratio>30) || (IRStruct.ratio<7) || (IRStruct.df>2.0) || (IRStruct.df<0.2) )
							{
								continue;
							}
							else 
							{
								Results.target    = desired; 
								Results.freq      = freq;
								Results.errorPPM	 = freq_err / desired * 1.0e6 ;
								Results.VCO_Div   = v;
								Results.refDiv    = r;
								Results.outDiv    = od; 
								Results.failed = FALSE;
								goto finished;								
							}
						}//end for(j=0;j<20;j++)
					}//end for(i=0;i<4;i++)
				}
			}//end of for( v = V_Divstart; v < max_V; v++ )

			if(od<=1030)
				od--;
			else if(od<=2060)
				od=od-2;
			else if(od<=4120)
				od=od-4;
			else od=od-8;

		}//end of while(od <= 8232)
	}//end of for( r = maxR, *saved_result_num = 0; r >= minR; r-- )

	ppm++;
	if(ppm>requestedppm)
	{
		return 2;
	}
	else 
	{
//		DbgPrint("ICS30703: increasing ppm to %d\n",ppm);
		goto increaseppm;
	}

finished:
	
	memcpy(theOne,&Results,sizeof(struct ResultStruct));

	memcpy(theOther,&IRStruct,sizeof(struct IcpRsStruct));
/*	
	DbgPrint("ICS30703: Best result is \n");
	DbgPrint("\tRD = %4i,",Results.refDiv);
	DbgPrint(" VD = %4i,",Results.VCO_Div);
	DbgPrint(" OD = %4i,",Results.outDiv);
	DbgPrint(" freq_Hz = %ld,\n",(ULONG)Results.freq);
	
	DbgPrint("\tRs = %5d, ",IRStruct.Rs);
	DbgPrint("Icp = %d, ",(ULONG)(IRStruct.icp*1e6));
	//	DbgPrint("pdf = %d, ",(ULONG)IRStruct.pdf);
	//	DbgPrint("nbw = %d, ",(ULONG)IRStruct.nbw);
	DbgPrint("ratio = %d, ",(ULONG)IRStruct.ratio);
	DbgPrint("df = %d\n",(ULONG)IRStruct.df*1000);
*/	
//	DbgPrint("ICS307-03 freq_Hz = %ld,\n",(ULONG)Results.freq);
/*
first, choose the best dividers (V, R, and OD) with

1st key best accuracy
2nd key lowest reference divide
3rd key highest VCO frequency (OD)

then choose the best loop filter with

1st key best PDF/NBW ratio (between 7 and 30, 15 is optimal)
2nd key best damping factor (between 0.2 and 2, 0.7 is optimal)
*/
/* this is 1MHz
	progdata[19]=0xff;
	progdata[18]=0xff;
	progdata[17]=0xff;
	progdata[16]=0xf0;
	progdata[15]=0x00;
	progdata[14]=0x01;
	progdata[13]=0x43;
	progdata[12]=0x1a;
	progdata[11]=0x9c;
	progdata[10]=0x00;
	progdata[9]=0x00;
	progdata[8]=0x00;
	progdata[7]=0x00;
	progdata[6]=0x00;
	progdata[5]=0x00;
	progdata[4]=0x00;
	progdata[3]=0x00;
	progdata[2]=0x0c;
	progdata[1]=0xdf;
	progdata[0]=0xee;
	goto doitnow;
*/
/* 10 MHz
	progdata[19]=0xff;
	progdata[18]=0xff;
	progdata[17]=0xff;
	progdata[16]=0x00;
	progdata[15]=0x80;
	progdata[14]=0x01;
	progdata[13]=0x00;
	progdata[12]=0x66;
	progdata[11]=0x38;
	progdata[10]=0x00;
	progdata[9]=0x00;
	progdata[8]=0x00;
	progdata[7]=0x00;
	progdata[6]=0x00;
	progdata[5]=0x00;
	progdata[4]=0x00;
	progdata[3]=0x00;
	progdata[2]=0x07;
	progdata[1]=0x20;
	progdata[0]=0x12;
	goto doitnow;
*/

	progdata[19]=0xff;
	progdata[18]=0xff;
	progdata[17]=0xff;
	progdata[16]=0x00;
	progdata[15]=0x00;
	progdata[14]=0x00;
	progdata[13]=0x00;
	progdata[12]=0x00;
	progdata[11]=0x00;
	progdata[10]=0x00;
	progdata[9]=0x00;
	progdata[8]=0x00;
	progdata[7]=0x00;
	progdata[6]=0x00;
	progdata[5]=0x00;
	progdata[4]=0x00;
	progdata[3]=0x00;
	progdata[2]=0x00;
	progdata[1]=0x00;
	progdata[0]=0x00;

//	progdata[16]|=0x02;	//enable CLK3
//	progdata[15]&=0xef;	//CLK3 source select: 1=CLK1, 0=CLK1 before OD 
//	progdata[15]|=0x08;	//CLK2 source select: 1=CLK1, 0=CLK1 before OD
//	progdata[15]|=0x40;	//reference source is: 1=crystal, 0=clock
	progdata[14]|=0x01;	//1=Power up, 0=power down feedback counter, charge pump and VCO
//	progdata[13]|=0x80;	//enable CLK2
	progdata[13]|=0x40;	//enable CLK1

	InputDivider = theOne->refDiv;
	VCODivider = theOne->VCO_Div;
	ChargePumpCurrent = theOther->icpnum;
	LoopFilterResistor = theOther->Rs;
	OutputDividerOut1 = theOne->outDiv;

	//InputDivider=2;
	//VCODivider=60;
	//OutputDividerOut1 = 45;
	//LoopFilterResistor=16000;
	//ChargePumpCurrent=3500;

	/* Table 1: Input Divider */

	if( (InputDivider==1)||(InputDivider==2) )
	{
		switch(InputDivider)
		{
		case 1:
			progdata[0]&=0xFC;
			progdata[1]&=0xF0;
			break;
		case 2:
			progdata[0]&=0xFC;
			progdata[0]|=0x01;
			progdata[1]&=0xF0;
			break;
		}
//	printf("1 0x%2.2X,0x%2.2X\n",progdata[1],progdata[0]);
	}
	else if( (InputDivider>=3) && (InputDivider<=17) )
	{
		temp=~(InputDivider-2);
		temp = (temp << 2);
		progdata[0]=(unsigned char)temp&0xff;

		progdata[0]&=0x3e;	//set bit 0 to a 0
		progdata[0]|=0x02;	//set bit 1 to a 1

//		printf("2 0x%2.2X,0x%2.2X\n",progdata[1],progdata[0]);
	}
	else if( (InputDivider>=18) && (InputDivider<=2055) )
	{
		temp=InputDivider-8;
		temp = (temp << 2);
		progdata[0]=(unsigned char)temp&0xff;
		progdata[1]=(unsigned char)((temp>>8)&0xff);

		progdata[0]|=0x03;	//set bit 0 and 1 to a 1

//		printf("3 0x%2.2X,0x%2.2X\n",progdata[1],progdata[0]);

	}
	else 
		return 3;

	/* Table 2 VCO Divider */

	if( (VCODivider >= 12) && (VCODivider <=2055) )
	{
		temp=VCODivider-8;
		temp=(temp << 5);
		progdata[1]|=temp&0xff;
		progdata[2]|=((temp>>8)&0xff);
//		printf("4 0x%2.2X,0x%2.2X\n",progdata[2],progdata[1]);
	}
	else return 4;

	/* Table 4 Loop Filter Resistor */

	switch(LoopFilterResistor)
	{
	case 64000:
		progdata[11]&=0xf9;	//bit 89 and 90 = 0
		break;
	case 52000:
		progdata[11]&=0xf9;	//bit 89 = 0
		progdata[11]|=0x04;	//bit 90 = 1
		break;
	case 16000:
		progdata[11]&=0xf9;	//bit 90 = 0
		progdata[11]|=0x02;	//bit 89 = 1
		break;
	case 4000:
		progdata[11]|=0x06;	//bit 89 and 90 = 1
		break;
	default:
		return 5;
	}
//	printf("5 0x%2.2X\n",progdata[11]);

	/* Table 3 Charge Pump Current */

	switch(ChargePumpCurrent)
	{
	case 125:
		progdata[11]|=0x38;
		progdata[15]&=0x7f;
		progdata[16]&=0xfe;
//		printf("125\n");
		break;

	case 250:
		progdata[11]|=0x38;
		progdata[15]|=0x80;
		progdata[16]&=0xfe;
		break;

	case 375:
		progdata[11]|=0x38;
		progdata[15]&=0x7f;
		progdata[16]|=0x01;
		break;

	case 500:
		progdata[11]|=0x38;
		progdata[15]|=0x80;
		progdata[16]|=0x01;				
		break;

	case 625:
		progdata[11]|=0x18;
		progdata[11]&=0xdf;
		progdata[15]&=0x7f;
		progdata[16]&=0xfe;
		break;

	case 750:
		progdata[11]|=0x10;
		progdata[11]&=0xd7;
		progdata[15]&=0x7f;
		progdata[16]&=0xfe;
		break;

	case 875:
		progdata[11]|=0x08;
		progdata[11]&=0xcf;
		progdata[15]&=0x7f;
		progdata[16]&=0xfe;
		break;

	case 1000:
		progdata[11]&=0xc7;
		progdata[15]&=0x7f;
		progdata[16]&=0xfe;
		break;

	case 1125:
		progdata[11]|=0x28;
		progdata[11]&=0xef;
		progdata[15]&=0x7f;
		progdata[16]|=0x01;
		break;

	case 1250:
		progdata[11]|=0x18;
		progdata[11]&=0xdf;
		progdata[15]|=0x80;
		progdata[16]&=0xfe;
		break;

	case 1500:
		progdata[11]|=0x28;
		progdata[11]&=0xef;
		progdata[15]|=0x80;
		progdata[16]|=0x01;				
		break;

	case 1750:
		progdata[11]|=0x08;
		progdata[11]&=0xcf;
		progdata[15]|=0x80;
		progdata[16]&=0xfe;
		break;

	case 1875:
		progdata[11]|=0x18;
		progdata[11]&=0xdf;
		progdata[15]&=0x7f;
		progdata[16]|=0x01;
		break;

	case 2000:
		progdata[11]&=0xc7;
		progdata[15]|=0x80;
		progdata[16]&=0xfe;
		break;

	case 2250:
		progdata[11]|=0x10;
		progdata[15]&=0x7f;
		progdata[16]|=0x01;
		break;

	case 2500:
		progdata[11]|=0x18;
		progdata[11]&=0xdf;
		progdata[15]|=0x80;
		progdata[16]|=0x01;
		break;

	case 2625:
		progdata[11]|=0x08;
		progdata[11]&=0xcf;
		progdata[15]&=0x7f;
		progdata[16]|=0x01;
		break;

	case 3000:
		progdata[11]&=0xc7;
		progdata[15]&=0x7f;
		progdata[16]|=0x01;
		break;

	case 3500:
		progdata[11]|=0x08;
		progdata[11]&=0xcf;
		progdata[15]|=0x80;
		progdata[16]|=0x01;
		break;

	case 4000:
		progdata[11]&=0xc7;
		progdata[15]|=0x80;
		progdata[16]|=0x01;
		break;

	default:
		return 6;
	}//end switch(j)
//	printf("6 0x%2.2X, 0x%2.2X, 0x%2.2X\n",progdata[16],progdata[15],progdata[11]);

	/* Table 5 Output Divider for Output 1 */
//OutputDividerOut1=38;
	if( (OutputDividerOut1 >= 2) && (OutputDividerOut1 <= 8232) )
	{
		switch(OutputDividerOut1)
		{
		case 2:
			progdata[11]&=0x7f;
			progdata[12]&=0x00;
			progdata[13]&=0xc0;
			break;

		case 3:
			progdata[11]|=0x80;
			progdata[12]&=0x00;
			progdata[13]&=0xc0;
			break;

		case 4:
			progdata[11]&=0x7f;
			progdata[12]|=0x04;
			progdata[13]&=0xc0;
			break;

		case 5:
			progdata[11]&=0x7f;
			progdata[12]|=0x01;
			progdata[13]&=0xc0;
			break;

		case 6:
			progdata[11]|=0x80;
			progdata[12]|=0x04;
			progdata[13]&=0xc0;
			break;

		case 7:
			progdata[11]|=0x80;
			progdata[12]|=0x01;
			progdata[13]&=0xc0;
			break;

		case 11:
			progdata[11]|=0x80;
			progdata[12]|=0x09;
			progdata[13]&=0xc0;
			break;

		case 9:
			progdata[11]|=0x80;
			progdata[12]|=0x05;
			progdata[13]&=0xc0;
			break;

		case 13:
			progdata[11]|=0x80;
			progdata[12]|=0x0d;
			progdata[13]&=0xc0;
			break;

		case 8: case 10: case 12: case 14: case 15: case 16: case 17:case 18: case 19:
		case 20: case 21: case 22: case 23: case 24: case 25: case 26: case 27: case 28:
		case 29: case 30: case 31: case 32: case 33: case 34: case 35: case 36: case 37:
			temp = ~(OutputDividerOut1-6);
			temp = (temp << 2);
			progdata[12] = (unsigned char)temp & 0x7f;
			
			progdata[11]&=0x7f;
			progdata[12]&=0xfe;
			progdata[12]|=0x02;
			progdata[13]&=0xc0;
			break;

		default:

			for(i=0;i<512;i++)
			{
				if( OutputDividerOut1 == ((((i+3)*2)+0)*(1)) )
				{
//					printf("1 x=%d, y=0, z=0\n",i);
//					DbgPrint("outputdivider1\n");
					temp = (i<< 5);
					progdata[12]|=(temp & 0xff);
					progdata[13]|=(temp >> 8)&0xff;

					progdata[12]&=0xe7;
					progdata[12]|=0x04;
					break;
				}
				
				else if( OutputDividerOut1 == ((((i+3)*2)+0)*(2)) )
				{
//					printf("2 x=%d, y=0, z=1\n",i);
					temp = (i<< 5);
					progdata[12]|=(temp & 0xff);
					progdata[13]|=(temp >> 8)&0xff;

					progdata[12]&=0xef;
					progdata[12]|=0x0c;
					break;
				}
					
				else if( OutputDividerOut1 == ((((i+3)*2)+0)*(4)) )
				{
//					printf("3 x=%d, y=0, z=2\n",i);
					temp = (i<< 5);
					progdata[12]|=(temp & 0xff);
					progdata[13]|=(temp >> 8)&0xff;

					progdata[12]&=0xf7;
					progdata[12]|=0x14;
					break;
				}
					
				else if( OutputDividerOut1 == ( (((i+3)*2)+0)*(8)) )
				{
//					printf("4 x=%d, y=0, z=3\n",i);
					temp = (i<< 5);
					progdata[12]|=(temp & 0xff);
					progdata[13]|=(temp >> 8)&0xff;

					progdata[12]|=0x1c;
					break;
				}

				else if( OutputDividerOut1 == ((((i+3)*2)+1)*(1)) )
				{
//					printf("5 x=%d, y=1, z=0\n",i);
//					DbgPrint("outputdivider5\n");
					temp = (i<< 5);
					progdata[12]|=(temp & 0xff);
					progdata[13]|=(temp >> 8)&0xff;

					progdata[12]&=0xe3;
					break;
				}
					
				else if( OutputDividerOut1 == ((((i+3)*2)+1)*(2)) )
				{
//					printf("6 x=%d, y=1, z=1\n",i);
					temp = (i<< 5);
					progdata[12]|=(temp & 0xff);
					progdata[13]|=(temp >> 8)&0xff;

					progdata[12]&=0xeb;	//power of 1
					progdata[12]|=0x08;
					break;
				}

				else if( OutputDividerOut1 == ((((i+3)*2)+1)*(4)) )
				{
//					printf("7 x=%d, y=1, z=2\n",i);
					temp = (i<< 5);
					progdata[12]|=(temp & 0xff);
					progdata[13]|=(temp >> 8)&0xff;

					progdata[12]&=0xf7;
					progdata[12]|=0x10;
					break;
				}

				else if( OutputDividerOut1 == ((((i+3)*2)+1)*(8)) )
				{
//					printf("8 x=%d, y=1, z=3\n",i);
					temp = (i<< 5);
					progdata[12]|=(temp & 0xff);
					progdata[13]|=(temp >> 8)&0xff;

					progdata[12]&=0xfb;
					progdata[12]|=0x18;
					break;
				}
			}
			progdata[11]|=0x80;	//1
			progdata[12]&=0xfe;	//0
			progdata[12]|=0x02;	//1
		}
//		printf("0x%2.2x, 0x%2.2x, 0x%2.2x\n\n",progdata[13]&0x3f, progdata[12], progdata[11]&0x80);
	}
	else return 7;
//doitnow:

/*	progdata[15]|=0x03;	//this will set
	progdata[14]|=0xc0;	//the OD of clock 3
	progdata[11]&=0xbf;	//to 2
*/
	return 0;

}
//end of GetICS30703Bits

#endif
#define WRITE_ANY WRITE_PORT_ULONG
#define READ_ANY READ_PORT_ULONG
void DELAY()
{
	KeStallExecutionProcessor(50);
}

int SetICS30703Bits(PULONG port,unsigned char *progdata,PSERIAL_DEVICE_EXTENSION extension)
{
	unsigned long tempValue = 0;
	unsigned long data=0;
	unsigned long savedval;
	unsigned long  i,j;
	KIRQL oldIrql;


 KeAcquireSpinLock(extension->pboardlock, &oldIrql);





	savedval = READ_ANY(port);
	savedval = savedval&0xfffffff0;
	WRITE_ANY(port,savedval&0xfffffff0);
	
//	DbgPrint("ICS30703: Programming word is: \t0x" );
	for(i=20;i>0;i--)
	{
//		DbgPrint(":%2.2X", progdata[i-1]);
		for(j=0;j<8;j++)
		{
			if(progdata[i-1]&0x80)
			{
				data = savedval|0x01;
				//DbgPrint("1");
			}
			else
			{
				data = savedval;
				//DbgPrint("0");
			}
			WRITE_ANY(port,data);
			DELAY();
			
			//clock high, data still there
			data |= 0x02;
			WRITE_ANY(port,data);
			DELAY();
			//clock low, data still there
			data &= 0xFFFFFFFd;
			WRITE_ANY(port,data);
			DELAY();
			
			
			progdata[i-1]=progdata[i-1] << 1;
			
//			printf(" sclk ");
		}
		
	}
//	DbgPrint("\n");
	data = savedval|0x8;		//strobe on
	WRITE_ANY(port,data);
	DELAY();	
	data = savedval;		//all off
	WRITE_ANY(port,data);
	DELAY();
	
//	printf("cs\n");
	

KeReleaseSpinLock(extension->pboardlock, oldIrql);	
	return 0;
	

	
}//end of SetICS30703Bits

int SetICS30703Bits2(PULONG port,unsigned char *progdata,PSERIAL_DEVICE_EXTENSION extension)
{
	unsigned long tempValue = 0;
	unsigned long data=0;
	unsigned long savedval;
	unsigned long  i,j;
	KIRQL oldIrql;


 KeAcquireSpinLock(extension->pboardlock, &oldIrql);


	savedval = READ_ANY(port);
	savedval = savedval&0xfffff0ff;
	WRITE_ANY(port,savedval&0xfffff0ff);
	
//	DbgPrint("ICS30703: Programming word is: \t0x" );
	for(i=20;i>0;i--)
	{
//		DbgPrint(":%2.2X", progdata[i-1]);
		for(j=0;j<8;j++)
		{
			if(progdata[i-1]&0x80)
			{
				data = savedval|0x0100;
				//DbgPrint("1");
			}
			else
			{
				data = savedval;
				//DbgPrint("0");
			}
			WRITE_ANY(port,data);
			DELAY();
			
			//clock high, data still there
			data |= 0x0200;
			WRITE_ANY(port,data);
			DELAY();
			//clock low, data still there
			data &= 0xFFFFFdFF;
			WRITE_ANY(port,data);
			DELAY();
			
			
			progdata[i-1]=progdata[i-1] << 1;
			
//			printf(" sclk ");
		}
		
	}
//	DbgPrint("\n");
	data = savedval|0x800;		//strobe on
	WRITE_ANY(port,data);
	DELAY();	
	data = savedval;		//all off
	WRITE_ANY(port,data);
	DELAY();
	
//	printf("cs\n");
KeReleaseSpinLock(extension->pboardlock, oldIrql);		
	return 0;
	
	
}//end of SetICS30703Bits


/*++

  Routine Description:
	Set the frequency of the clock generator.  It will detect which clock generator
	is present using the MPIO bits 3,4 & 5.  It will then set the clock generator to
	be the closest it can get to what is actually desired.

  Arguments:
	Context - The device extension for serial device
	being managed.
	rate - The desired clock frequency.

  Return Value:
	TRUE - If the clock setting has executed successfully.
	FALSE - If the clock setting terminated due to an error or problem.

--*/
#define DBGP 
#if 0
BOOLEAN SerialSetClock(IN PVOID Context, IN ULONG *rate)
{
#define STARTWRD 0x1e05
#define MIDWRD   0x1e04
#define ENDWRD   0x1e00
	
	PSERIAL_DEVICE_EXTENSION extension;
	
	unsigned long 
		bestVDW=1,	//Best calculated VCO Divider Word
		bestRDW=1,	//Best calculated Reference Divider Word
		bestOD=1,	//Best calculated Output Divider
		result=0,
		t=0,
		i=0,
		j=0,
		tempValue=0,
		offset=0,
		temp=0,
		board=0,
		vdw=1,		//VCO Divider Word
		rdw=1,		//Reference Divider Word
		od=1,		//Output Divider
		lVDW=1,		//Lowest vdw
		lRDW=1,		//Lowest rdw
		lOD=1,		//Lowest OD
		hVDW=1,		//Highest vdw
		hRDW=1,		//Highest rdw
		hOD=1,		//Highest OD
		desired=0,	//Desired clock 1 output
		hi,		//initial range freq Max
		low,	//initial freq range Min
		check,		//Calculated clock
		clk1,		//Actual clock 1 output
		inFreq=18432000,	//Input clock frequency
		range1=0,		//Desired frequency range limit per ics307 mfg spec.
		range2=0;		////Desired frequency range limit per ics307 mfg spec.
	UCHAR data;
	PUCHAR base_add;
	ULONG P;
	ULONG Pprime;
	ULONG Q;
	ULONG Qprime;
	ULONG M;
	ULONG I;
	ULONG D = 0;	//from bitcalc, but the datasheet says to make this 1...
	ULONG fvco;
	ULONG bestP = 0;
	ULONG bestQ = 0;
	ULONG bestM;
	ULONG bestI;
	ULONG Progword;
	ULONG Stuffedword;
	ULONG bit1;
	ULONG bit2;
	ULONG bit3;
	ULONG rangelo = 19;	//		rangelo = (inFreq/1000000) +1;	
	ULONG rangehi = 92;	//		rangehi = (inFreq/200000);
	ULONG best_diff = 1000000;
	USHORT nmbits;
	PUCHAR clockreg;
	
	
	
	extension = (PSERIAL_DEVICE_EXTENSION)Context;
	
	base_add = extension->Controller;
	clockreg = extension->clockreg_add;
	if(base_add==0) return FALSE;
	if(clockreg==0) return FALSE;
	
	//	DbgPrint("SerialSetClock %d\n",rate[0]);
	//	DBGP("baseaddress = %8.8x \n", base_add);
	//	DBGP("clockaddress = %8.8x \n", clockreg);
	
	data = 0;
	board = 0;
	offset = 0;
	desired = *rate;
	
	//Begin case for ICD2053B
	DBGP("Into 2053b case");
	while ((desired < 1000000 )||(desired>60000000))
	{
		if (desired < 1000000 )
		{
			DBGP("The rate is: %d\n",desired);
			*rate=(*rate)*2;
			desired = *rate;
		}
		else
		{
			DBGP("The rate %d is too high.  Aborting.\n",desired);
			return FALSE;
		}
	}
	
	check = 1000000;		//hopefully we can do better than this
	for(i=0;i<=7;i++)
	{
		M = i;
		fvco = (desired * (1<<i));
		if(fvco<80000000) I = 0x00000000;
		if(fvco>=80000000) I = 0x00000008;
		if((fvco>50000000)&&(fvco<150000000))
		{
			DBGP("fvco = %d\n",fvco);
			for(P=4;P<=130;P++)
				for(Q=rangelo;Q<=rangehi;Q++)
				{
					clk1 = (((2 * inFreq * P) / Q) / (1 << i));
					if(clk1==desired) 
					{
						DBGP("%u,%u\n",P,Q);
						//MessageBox(NULL,buf,"Direct Hit",MB_OK);
						bestP = P;
						bestQ = Q;
						bestM = M;
						bestI = I;
						check = clk1;
						goto donecalc;
					}
					else 
					{
						if(((ULONG)(labs(desired - clk1)))<(best_diff)) 
						{
							best_diff = labs(desired - clk1);
							check = clk1;
							bestP = P;
							bestQ = Q;
							bestM = M;
							bestI = I;
							DBGP("desired:%d,actual:%d, best%d P%u,Q%u,fvco:%d,M:%u\n",desired,clk1,best_diff,bestP,bestQ,fvco,M);
							//MessageBox(buf,"ratiocalc",MB_OK);
						}
					}	
				}
		}
	}
donecalc:
	if((bestP!=0)&&(bestQ!=0))
	{
		//here bestP BestQ are good to go.
		I = bestI;
		M = bestM;
		P = bestP;
		Q = bestQ;
		Pprime = bestP - 3;
		Qprime = bestQ - 2;
		DBGP("P':%u, Q':%u, M:%u, I:%u\n",Pprime,Qprime,M,I);
		//MessageBox(buf,"P,Q,M,I",MB_OK);
		Progword = 0;
		Progword =  (Pprime<<15) | (D<<14) | (M<<11) | (Qprime<<4) | I;
		DBGP("%lx\n",Progword);
		//	MessageBox(buf,"Progword",MB_OK);
		bit1 = 0;
		bit2 = 0;
		bit3 = 0;
		Stuffedword = 0;
		i = 0;
		j = 0;
		bit1 = ((Progword>>i)&1);
		Stuffedword |=  (bit1<<j);
		i++;
		j++;
		bit2 = ((Progword>>i)&1);
		Stuffedword |=  (bit2<<j);
		i++;
		j++;
		bit3 = ((Progword>>i)&1);
		Stuffedword |=  (bit3<<j);
		j++;
		i++;
		while(i<=22)
		{
			if((bit1==1)&&(bit2==1)&&(bit3==1))
			{
				//force a 0 in the stuffed word;
				j++;
				bit3 = 0;
				DBGP("i,j : %u,%u\n",i,j);
				//			MessageBox(buf,"Stuffing",MB_OK);
			}
			bit1 = bit2;
			bit2 = bit3;
			bit3 = ((Progword>>i)&1);
			Stuffedword |=  (bit3<<j);
			i++;
			j++;
		}
		nmbits = (USHORT)j-1;
		DBGP("SW:%lx ,numbits:%u\n",Stuffedword,j);
		//			clk->clockbits = Stuffedword;
		//			clk->actualclock = ((2.0 * inFreq) * ((double)P/(double)Q)) / pow(2,M);
		//			*m_actual_clock = ((2.0 * inFreq) * ((double)P/(double)Q)) / pow(2,M);
		//	MessageBox(buf,"stuffedword, numbits",MB_OK);
	}
	else
	{
		DBGP("\r\nError in ICD calculation\r\n");
		return FALSE;
	}
	
	/*********************** Begin clock set *************************/
	
	set_clock_generator(clockreg,Stuffedword,nmbits);
	
	if(1)
	{
		//ok, here we have the clock set, lets find all the cards with the same clock address
		//and make their ClockRate matchup.
		
		PSERIAL_DEVICE_EXTENSION pExtension;
		PLIST_ENTRY pCurDevObj;
		
		//
		// Loop through all previously attached devices
		//
		if (!IsListEmpty(&SerialGlobals.AllDevObjs)) {
			pCurDevObj = SerialGlobals.AllDevObjs.Flink;
			pExtension = CONTAINING_RECORD(pCurDevObj, SERIAL_DEVICE_EXTENSION,
				AllDevObjs);
		} else {
			pCurDevObj = NULL;
			pExtension = NULL;
		}
		if(pExtension!=NULL)
		{
			//
			do {
				if(pExtension->clockreg_add==clockreg) pExtension->ClockRate = rate[0];
				
				pCurDevObj = pCurDevObj->Flink;
				if (pCurDevObj != NULL) {
					pExtension = CONTAINING_RECORD(pCurDevObj,SERIAL_DEVICE_EXTENSION,
						AllDevObjs);
				}
				
			} while (pCurDevObj != NULL && pCurDevObj != &SerialGlobals.AllDevObjs);
		}
		
	}
	
	//		break;	//End case for ICD2053b clock
	//End Clock select switch statement.
	return TRUE;
}

void set_clock_generator(PUCHAR port, ULONG hval,ULONG nmbits)
{
	
	ULONG curval;
	ULONG tempval;
	ULONG i;
	
	//DbgPrint("Set_clock_generator %x %x %d\n",port,hval,nmbits);
	
	curval = 0;
	//bit 0 = data
	//bit 1 = clock;
	WRITE_PORT_UCHAR(port,(UCHAR)curval);
	//SimplDrvKdPrint(("6"));
	
	tempval = STARTWRD;
	for(i=0;i<14;i++)
	{
		curval = 0;
		curval = (char)(tempval&0x1);   //set bit
		WRITE_PORT_UCHAR(port,(UCHAR)curval);
		curval = curval |0x02;          //force rising edge
		WRITE_PORT_UCHAR(port,(UCHAR)curval);              //clock in data
		curval = curval &0x01;          //force falling edge
		WRITE_PORT_UCHAR(port,(UCHAR)curval);              //set clock low
		tempval = tempval >> 1;         //get next bit
	}
	//SimplDrvKdPrint(("7"));	
	tempval = hval;
	for(i=0;i<nmbits;i++)
	{
		curval = 0;
		curval = (char)(tempval&0x1);   //set bit
		WRITE_PORT_UCHAR(port,(UCHAR)curval);
		curval = curval |0x02;          //force rising edge
		WRITE_PORT_UCHAR(port,(UCHAR)curval);              //clock in data
		curval = curval &0x01;          //force falling edge
		WRITE_PORT_UCHAR(port,(UCHAR)curval);              //set clock low
		tempval = tempval >> 1;         //get next bit
	}
	//SimplDrvKdPrint(("8"));
	tempval = MIDWRD;
	for(i=0;i<14;i++)
	{
		curval = 0;
		curval = (char)(tempval&0x1);   //set bit
		WRITE_PORT_UCHAR(port,(UCHAR)curval);
		curval = curval |0x02;          //force rising edge
		WRITE_PORT_UCHAR(port,(UCHAR)curval);              //clock in data
		curval = curval &0x01;          //force falling edge
		WRITE_PORT_UCHAR(port,(UCHAR)curval);              //set clock low
		tempval = tempval >> 1;         //get next bit
	}
	//SimplDrvKdPrint(("9"));
	//pause for >10ms --should be replaced with a regulation pause routine
	DBGP("Pause here");
	for(i=0;i<200;i++)KeStallExecutionProcessor(50);
	//SimplDrvKdPrint(("10"));
	tempval = ENDWRD;
	for(i=0;i<14;i++)
	{
		curval = 0;
		curval = (char)(tempval&0x1);   //set bit
		WRITE_PORT_UCHAR(port,(UCHAR)curval);
		curval = curval |0x02;          //force rising edge
		WRITE_PORT_UCHAR(port,(UCHAR)curval);              //clock in data
		curval = curval &0x01;          //force falling edge
		WRITE_PORT_UCHAR(port,(UCHAR)curval);              //set clock low
		tempval = tempval >> 1;         //get next bit
	}
	//SimplDrvKdPrint(("11"));
}
#endif

int uarttype_850_or_950(PUCHAR port)
{
	int uarttype;
	char byte1,byte2,byte3,byte4,saved,saved2,saved3,saved4,saved5;
	//make this a registry entry!!!?
	//this detection code is bad!
	saved = READ_PORT_UCHAR(port+3);
	WRITE_PORT_UCHAR(port+3,0x80);
	saved2 = READ_PORT_UCHAR(port+0);
	
	WRITE_PORT_UCHAR(port+0,0x01);
	
	
	WRITE_PORT_UCHAR(port+7,0x00);
	WRITE_PORT_UCHAR(port+5,0xC0);
	WRITE_PORT_UCHAR(port+7,0x08);
	byte1 = READ_PORT_UCHAR(port+5);
	WRITE_PORT_UCHAR(port+7,0x09);
	byte2 = READ_PORT_UCHAR(port+5);
	WRITE_PORT_UCHAR(port+7,0x0a);
	byte3 = READ_PORT_UCHAR(port+5);
	WRITE_PORT_UCHAR(port+7,0x0b);
	byte4 = READ_PORT_UCHAR(port+5);
	WRITE_PORT_UCHAR(port+7,0x00);
	WRITE_PORT_UCHAR(port+5,0x00);//turn off icr read enable
	
	DbgPrint("950detect:%x:%x:%x\n",byte1,byte2,byte3);
	if(((byte1&0xff)==0x16)&&((byte2&0xff)==0xC9)&&((byte3&0xff)==0x50))
	{
		DbgPrint("possibly a 16C950?\n");
		uarttype=1;
		//probably a 16c950
		WRITE_PORT_UCHAR(port+0,saved2);
		WRITE_PORT_UCHAR(port+3,saved);
		return 1;			
	}
	if(((byte1&0xff)==0x16)&&((byte2&0xff)==0xC9)&&((byte3&0xff)==0x54))
	{
		DbgPrint("possibly a 16C954?\n");
		uarttype=1;
		WRITE_PORT_UCHAR(port+0,saved2);
		WRITE_PORT_UCHAR(port+3,saved);
		return 1;			
		
		//probably a 16c950
	}
	
	
	WRITE_PORT_UCHAR(port+3,0x80);
	WRITE_PORT_UCHAR(port+0,0x0);
	saved3 = READ_PORT_UCHAR(port+1);
	WRITE_PORT_UCHAR(port+1,0x0);
	byte1 = READ_PORT_UCHAR(port+1);
	byte2 = READ_PORT_UCHAR(port+0);
	WRITE_PORT_UCHAR(port+0,saved2);
	WRITE_PORT_UCHAR(port+1,saved3);
	//		DbgPrint("850detect:%x\n",byte1);
	if((byte1&0xff)==0x10)
	{
		DbgPrint("possibly a 16C850?\n");
		uarttype=0;
		//probably a 16C850
	}
	if((byte1&0xff)==0x14)
	{
		uarttype=0;
		//probably a 16C864 or 16C854
		//                        DbgPrint("possibly a 16C854 or 16C864?\n");
	}
	WRITE_PORT_UCHAR(port+3,saved);
	return uarttype;
}

BOOLEAN  setisosync(PVOID Context)
{
	ULONG savereg;
	PUCHAR port;
	ULONG val;
	PSERIAL_DEVICE_EXTENSION extension;
	
	extension = (PSERIAL_DEVICE_EXTENSION)Context;
	port = extension->Controller;
	val = extension->Isosync;
	
	if(port==0)return FALSE; 				
	savereg = READ_PORT_UCHAR(port+3);
	WRITE_PORT_UCHAR(port+3,0xbf);
	WRITE_PORT_UCHAR(port+2,0x10);//turn on enhanced mode
	
	WRITE_PORT_UCHAR(port+3,0);
	WRITE_PORT_UCHAR(port+7,3);//CKS register
	
	if(val==0) WRITE_PORT_UCHAR(port+5,0x00);//normal 550 mode
	else if(val==1)	WRITE_PORT_UCHAR(port+5,(UCHAR)(0xDD));//1x iso sync mode, tx clocked 1x by RI input,rx clocked by dsr clock retransmitted on DTR
	else if(val==2)	WRITE_PORT_UCHAR(port+5,(UCHAR)(0x19));//1x rx iso sync mode, tx clocked internal bgr, rx clocked by dsr clock retransmitted on DTR
	
	WRITE_PORT_UCHAR(port+3,0xbf);
	WRITE_PORT_UCHAR(port+2,0x00);//clear the EFR bit (Latch extendeds);
	
	WRITE_PORT_UCHAR(port+3,(UCHAR)savereg&0x7f);
	
	if(val==0)
	{
		WRITE_PORT_UCHAR(port+7,0x0e);//put MDM address (0x0e) into the SPR
		WRITE_PORT_UCHAR(port+0x05,0x00);//MDM register normal
	}
	else if(val==1)
	{
		WRITE_PORT_UCHAR(port+7,0x0e);//put MDM address (0x0e) into the SPR
		WRITE_PORT_UCHAR(port+0x05,0x06);//MDM register disable RI & DSR interrutps
	}
	else if(val==2)
	{
		WRITE_PORT_UCHAR(port+7,0x0e);//put MDM address (0x0e) into the SPR
		WRITE_PORT_UCHAR(port+0x05,0x02);//MDM register disable DSR interrupt
	}
	
	WRITE_PORT_UCHAR(port+3,(UCHAR)savereg);
	
	return TRUE;
}

BOOLEAN  setspecial1xclkdtr(PVOID Context)
{
	ULONG savereg;
	PUCHAR port;
	ULONG val;
	PSERIAL_DEVICE_EXTENSION extension;
	extension = (PSERIAL_DEVICE_EXTENSION)Context;
	port = extension->Controller;
	val = extension->clk1xdtr;
	
	if(port==0)return FALSE; 				
	if(extension->type850or950==1)
	{

		UCHAR temp;
		//DbgPrint("950\n");
		//DbgPrint("setdtr1x port:%x val:%x\n",port,val);
		
		savereg = READ_PORT_UCHAR(port+3);
		WRITE_PORT_UCHAR(port+3,0xbf);
		WRITE_PORT_UCHAR(port+2,0x10);//turn on enhanced mode
		
		WRITE_PORT_UCHAR(port+3,0);
		WRITE_PORT_UCHAR(port+7,3);//CKS register
		temp = 0x20;//allways assume triggers are active
		if(val==0)	WRITE_PORT_UCHAR(port+5,0x00);//normal 550 mode
		if(val==1)  WRITE_PORT_UCHAR(port+5,(UCHAR)(0x10));// tx clocked normal , rx normal (async) 1x clk transmitted on DTR
		
		WRITE_PORT_UCHAR(port+3,0xbf);
		WRITE_PORT_UCHAR(port+2,0x00);//clear the EFR bit (Latch extendeds);
		WRITE_PORT_UCHAR(port+3,(UCHAR)savereg);
	}
	else
	{
		//DbgPrint("850\n");
	}
	return TRUE;
}

BOOLEAN  setHardwareFlowControl(PVOID Context)
{
	ULONG savereg;
	PUCHAR port;
	ULONG value;
	PSERIAL_DEVICE_EXTENSION extension;
	extension = (PSERIAL_DEVICE_EXTENSION)Context;
	port = extension->Controller;
	value = extension->HardwareRTSCTS;
	
	if(port==0)return FALSE; 				
	if(extension->type850or950==1)
	{
		//DbgPrint("950\n");
		UCHAR temp;
		//DbgPrint("950\n");
//		DbgPrint("setdtr1x port:%x val:%x\n",port,val);
		
		savereg = READ_PORT_UCHAR(port+3);
		WRITE_PORT_UCHAR(port+3,0xbf);

		WRITE_PORT_UCHAR(port+2,0x10);//turn on enhanced mode

		switch (value)
		{
		case 0:
			WRITE_PORT_UCHAR(port+2,0x00);//turn off CTS+RTS flow control - EFR=0x00
			break;
		case 1:
			WRITE_PORT_UCHAR(port+2,0x50);//turn on RTS flow control - EFR=0x50 - Enhanced mode+RTS Flow
			break;
		case 2:
			WRITE_PORT_UCHAR(port+2,0x90);//turn on CTS flow control - EFR=0x90 - Enhanced mode+CTS Flow
			break;
		case 3:
			WRITE_PORT_UCHAR(port+2,0xd0);//turn on CTS+RTS flow control - EFR=0xd0 - Enhanced mode+CTS Flow+RTS Flow
			break;
		default:
			DbgPrint("%d, invalid selectiojn for HardwareRTSCTS\n",value);
		}
		
		WRITE_PORT_UCHAR(port+3,(UCHAR)savereg);
		DbgPrint("HardwareRTSCTS = %d\n",value);
	}
	else
	{
		//DbgPrint("850\n");
	}
	return TRUE;
}