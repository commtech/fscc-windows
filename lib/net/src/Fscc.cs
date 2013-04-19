using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using Microsoft.Win32.SafeHandles;
using System.Threading;
using System.IO.Ports;
using System.Reflection;

namespace Fscc
{
    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public struct Registers
    {
        /* BAR 0 */
        private Int64 __reserved11;
        private Int64 __reserved12;

        public Int64 FIFOT;

        private Int64 __reserved21;
        private Int64 __reserved22;

        public Int64 CMDR;
        public Int64 STAR; /* Read-only */
        public Int64 CCR0;
        public Int64 CCR1;
        public Int64 CCR2;
        public Int64 BGR;
        public Int64 SSR;
        public Int64 SMR;
        public Int64 TSR;
        public Int64 TMR;
        public Int64 RAR;
        public Int64 RAMR;
        public Int64 PPR;
        public Int64 TCR;
        public Int64 VSTR;

        private Int64 __reserved41;

        public Int64 IMR;
        public Int64 DPLLR;

        /* BAR 2 */
        public Int64 FCR;

        public Registers(bool init) 
        {
            const int FSCC_UPDATE_VALUE = -2;

            /* BAR 0 */
            __reserved11 = -1;
            __reserved12 = -1;

            FIFOT = FSCC_UPDATE_VALUE;

            __reserved21 = -1;
            __reserved22 = -1;

            CMDR = FSCC_UPDATE_VALUE;
            STAR = FSCC_UPDATE_VALUE; /* Read-only */
            CCR0 = FSCC_UPDATE_VALUE;
            CCR1 = FSCC_UPDATE_VALUE;
            CCR2 = FSCC_UPDATE_VALUE;
            BGR = FSCC_UPDATE_VALUE;
            SSR = FSCC_UPDATE_VALUE;
            SMR = FSCC_UPDATE_VALUE;
            TSR = FSCC_UPDATE_VALUE;
            TMR = FSCC_UPDATE_VALUE;
            RAR = FSCC_UPDATE_VALUE;
            RAMR = FSCC_UPDATE_VALUE;
            PPR = FSCC_UPDATE_VALUE;
            TCR = FSCC_UPDATE_VALUE;
            VSTR = FSCC_UPDATE_VALUE;

            __reserved41 = -1;

            IMR = FSCC_UPDATE_VALUE;
            DPLLR = FSCC_UPDATE_VALUE;

            /* BAR 2 */
            FCR = FSCC_UPDATE_VALUE;
        }
    };

    public enum TransmitModifiers { XF = 0, XREP = 1, TXT = 2, TXEXT = 4 };

    public class Port
    {
        const string DLL_PATH = "cfscc.dll";

        IntPtr Handle;
        uint port_num;

        public override string ToString()
        {
            return String.Format("FSCC{0}", port_num);
        }

        [DllImport(DLL_PATH, CallingConvention = CallingConvention.Cdecl)]
        private static extern int fscc_connect(uint port_num, bool overlapped, out IntPtr h);

        public Port(uint port_num)
        {
            int e = fscc_connect(port_num, false, out this.Handle);

            if (e >= 1)
                throw new Exception(e.ToString());

            this.port_num = port_num;
        }

        [DllImport(DLL_PATH, CallingConvention = CallingConvention.Cdecl)]
        private static extern int fscc_disconnect(IntPtr h);

        ~Port()
        {
            fscc_disconnect(this.Handle);
        }

        [DllImport(DLL_PATH, CallingConvention = CallingConvention.Cdecl)]
        private static extern int fscc_set_tx_modifiers(IntPtr h, uint modifiers);

        [DllImport(DLL_PATH, CallingConvention = CallingConvention.Cdecl)]
        private static extern int fscc_get_tx_modifiers(IntPtr h, out uint modifiers);

        public TransmitModifiers TxModifiers
        {
            set
            {
                int e = fscc_set_tx_modifiers(this.Handle, (uint)value);

                if (e >= 1)
                    throw new Exception(e.ToString());
            }

            get
            {
                uint modifiers;

                int e = fscc_get_tx_modifiers(this.Handle, out modifiers);

                if (e >= 1)
                    throw new Exception(e.ToString());

                return (TransmitModifiers)modifiers;
            }
        }

        [DllImport(DLL_PATH, CallingConvention = CallingConvention.Cdecl)]
        private static extern int fscc_enable_append_status(IntPtr h);

        [DllImport(DLL_PATH, CallingConvention = CallingConvention.Cdecl)]
        private static extern int fscc_disable_append_status(IntPtr h);

        [DllImport(DLL_PATH, CallingConvention = CallingConvention.Cdecl)]
        private static extern int fscc_get_append_status(IntPtr h, out bool status);

        public bool AppendStatus
        {
            set
            {
                int e = 0;

                if (value == true)
                    e = fscc_enable_append_status(this.Handle);
                else
                    e = fscc_disable_append_status(this.Handle);

                if (e >= 1)
                    throw new Exception(e.ToString());
            }

            get
            {
                bool status;

                int e = fscc_get_append_status(this.Handle, out status);

                if (e >= 1)
                    throw new Exception(e.ToString());

                return status;
            }
        }

        [DllImport(DLL_PATH, CallingConvention = CallingConvention.Cdecl)]
        private static extern int fscc_enable_ignore_timeout(IntPtr h);

        [DllImport(DLL_PATH, CallingConvention = CallingConvention.Cdecl)]
        private static extern int fscc_disable_ignore_timeout(IntPtr h);

        [DllImport(DLL_PATH, CallingConvention = CallingConvention.Cdecl)]
        private static extern int fscc_get_ignore_timeout(IntPtr h, out bool status);

        public bool IgnoreTimeout
        {
            set
            {
                int e = 0;

                if (value == true)
                    e = fscc_enable_ignore_timeout(this.Handle);
                else
                    e = fscc_disable_ignore_timeout(this.Handle);

                if (e >= 1)
                    throw new Exception(e.ToString());
            }

            get
            {
                bool status;

                int e = fscc_get_ignore_timeout(this.Handle, out status);

                if (e >= 1)
                    throw new Exception(e.ToString());

                return status;
            }
        }

        [DllImport(DLL_PATH, CallingConvention = CallingConvention.Cdecl)]
        private static extern int fscc_purge(IntPtr h, bool tx, bool rx);

        public void Purge(bool tx, bool rx)
        {
            fscc_purge(this.Handle, tx, rx);
        }

        [DllImport(DLL_PATH, CallingConvention = CallingConvention.Cdecl)]
        private static extern int fscc_set_clock_frequency(IntPtr h, uint frequency, uint ppm);

        public void SetClockFrequency(uint frequency)
        {
            int e = 0;

            e = fscc_set_clock_frequency(this.Handle, frequency, 2);

            if (e >= 1)
                throw new Exception(e.ToString());
        }

        public void SetClockFrequency(uint frequency, uint ppm)
        {
            SetClockFrequency(frequency);
        }

        [DllImport(DLL_PATH, CallingConvention = CallingConvention.Cdecl)]
        private static extern int fscc_write(IntPtr h, byte[] buf, uint size, out uint bytes_written, out NativeOverlapped overlapped);

        [DllImport(DLL_PATH, CallingConvention = CallingConvention.Cdecl)]
        private static extern int fscc_write(IntPtr h, byte[] buf, uint size, out uint bytes_written, IntPtr overlapped);

        public uint Write(byte[] buf, uint size, out NativeOverlapped o)
        {
            uint bytes_written;

            int e = fscc_write(this.Handle, buf, size, out bytes_written, out o);

            if (e >= 1)
                throw new Exception(e.ToString());

            return bytes_written;
        }

        public uint Write(byte[] buf, uint size)
        {
            uint bytes_written;

            int e = fscc_write(this.Handle, buf, size, out bytes_written, IntPtr.Zero);

            if (e >= 1)
                throw new Exception(e.ToString());

            return bytes_written;
        }

        public uint Write(string s)
        {
            return Write(Encoding.ASCII.GetBytes(s), (uint)s.Length);
        }

        [DllImport(DLL_PATH, CallingConvention = CallingConvention.Cdecl)]
        private static extern int fscc_read(IntPtr h, byte[] buf, uint size, out uint bytes_read, out NativeOverlapped overlapped);

        [DllImport(DLL_PATH, CallingConvention = CallingConvention.Cdecl)]
        private static extern int fscc_read(IntPtr h, byte[] buf, uint size, out uint bytes_read, IntPtr overlapped);

        public uint Read(byte[] buf, uint size, out NativeOverlapped o)
        {
            uint bytes_read;

            int e = fscc_read(this.Handle, buf, size, out bytes_read, out o);

            if (e >= 1)
                throw new Exception(e.ToString());

            return bytes_read;
        }

        public uint Read(byte[] buf, uint size)
        {
            uint bytes_read;

            int e = fscc_read(this.Handle, buf, size, out bytes_read, IntPtr.Zero);

            if (e >= 1)
                throw new Exception(e.ToString());

            return bytes_read;
        }

        public string Read(uint count)
        {
            System.Text.ASCIIEncoding encoder = new System.Text.ASCIIEncoding();
            byte[] input_bytes = new byte[count];
            uint bytes_read = 0;

            bytes_read = Read(input_bytes, (uint)input_bytes.Length);

            return encoder.GetString(input_bytes, 0, (int)bytes_read);
        }

        [DllImport(DLL_PATH, CallingConvention = CallingConvention.Cdecl)]
        private static extern int fscc_set_registers(IntPtr h, IntPtr registers);

        [DllImport(DLL_PATH, CallingConvention = CallingConvention.Cdecl)]
        private static extern int fscc_get_registers(IntPtr h, IntPtr registers);

        protected Registers Registers
        {
            set
            {
                IntPtr buffer = Marshal.AllocHGlobal(Marshal.SizeOf(value));
                Marshal.StructureToPtr(value, buffer, false);

                int e = fscc_set_registers(this.Handle, buffer);

                Marshal.FreeHGlobal(buffer);

                if (e >= 1)
                    throw new Exception(e.ToString());
            }

            get
            {
                Registers r = new Registers(true);

                IntPtr buffer = Marshal.AllocHGlobal(Marshal.SizeOf(r));
                Marshal.StructureToPtr(r, buffer, false);

                int e = fscc_get_registers(this.Handle, buffer);

                r = (Registers)Marshal.PtrToStructure(buffer, typeof(Registers));
                Marshal.FreeHGlobal(buffer);

                if (e >= 1)
                    throw new Exception(e.ToString());

                return r;
            }
        }

        public UInt32 FIFOT
        {
            set
            {
                Registers r = new Registers(true);

                r.FIFOT = value;

                this.Registers = r;
            }

            get
            {
                return (UInt32)this.Registers.FIFOT;
            }
        }

        public UInt32 CMDR
        {
            set
            {
                Registers r = new Registers(true);

                r.CMDR = value;

                this.Registers = r;
            }
        }

        public UInt32 STAR
        {
            get
            {
                return (UInt32)this.Registers.STAR;
            }
        }

        public UInt32 CCR0
        {
            set
            {
                Registers r = new Registers(true);

                r.CCR0 = value;

                this.Registers = r;
            }

            get
            {
                return (UInt32)this.Registers.CCR0;
            }
        }

        public UInt32 CCR1
        {
            set
            {
                Registers r = new Registers(true);

                r.CCR1 = value;

                this.Registers = r;
            }

            get
            {
                return (UInt32)this.Registers.CCR1;
            }
        }

        public UInt32 CCR2
        {
            set
            {
                Registers r = new Registers(true);

                r.CCR2 = value;

                this.Registers = r;
            }

            get
            {
                return (UInt32)this.Registers.CCR2;
            }
        }

        public UInt32 BGR
        {
            set
            {
                Registers r = new Registers(true);

                r.BGR = value;

                this.Registers = r;
            }

            get
            {
                return (UInt32)this.Registers.BGR;
            }
        }

        public UInt32 SSR
        {
            set
            {
                Registers r = new Registers(true);

                r.SSR = value;

                this.Registers = r;
            }

            get
            {
                return (UInt32)this.Registers.SSR;
            }
        }

        public UInt32 SMR
        {
            set
            {
                Registers r = new Registers(true);

                r.SMR = value;

                this.Registers = r;
            }

            get
            {
                return (UInt32)this.Registers.SMR;
            }
        }

        public UInt32 TSR
        {
            set
            {
                Registers r = new Registers(true);

                r.TSR = value;

                this.Registers = r;
            }

            get
            {
                return (UInt32)this.Registers.TSR;
            }
        }

        public UInt32 TMR
        {
            set
            {
                Registers r = new Registers(true);

                r.TMR = value;

                this.Registers = r;
            }

            get
            {
                return (UInt32)this.Registers.TMR;
            }
        }

        public UInt32 RAR
        {
            set
            {
                Registers r = new Registers(true);

                r.RAR = value;

                this.Registers = r;
            }

            get
            {
                return (UInt32)this.Registers.RAR;
            }
        }

        public UInt32 RAMR
        {
            set
            {
                Registers r = new Registers(true);

                r.RAMR = value;

                this.Registers = r;
            }

            get
            {
                return (UInt32)this.Registers.RAMR;
            }
        }

        public UInt32 PPR
        {
            set
            {
                Registers r = new Registers(true);

                r.PPR = value;

                this.Registers = r;
            }

            get
            {
                Registers r = this.Registers;

                return (UInt32)r.PPR;
            }
        }

        public UInt32 TCR
        {
            set
            {
                Registers r = new Registers(true);

                r.TCR = value;

                this.Registers = r;
            }

            get
            {
                return (UInt32)this.Registers.TCR;
            }
        }

        public UInt32 VSTR
        {
            get
            {
                Registers r = this.Registers;

                return (UInt32)r.VSTR;
            }
        }

        public UInt32 IMR
        {
            set
            {
                Registers r = new Registers(true);

                r.IMR = value;

                this.Registers = r;
            }

            get
            {
                return (UInt32)this.Registers.IMR;
            }
        }

        public UInt32 DPLLR
        {
            set
            {
                Registers r = new Registers(true);

                r.DPLLR = value;

                this.Registers = r;
            }

            get
            {
                return (UInt32)this.Registers.DPLLR;
            }
        }

        public UInt32 FCR
        {
            set
            {
                Registers r = new Registers(true);

                r.FCR = value;

                this.Registers = r;
            }

            get
            {
                return (UInt32)this.Registers.FCR;
            }
        }

        public string Firmware
        {
            get
            {
                uint vstr = this.VSTR;

                byte PREV = (byte)((this.VSTR & 0x0000FF00) >> 8);
                byte FREV = (byte)((this.VSTR & 0x000000FF));

                return String.Format("{0:X}.{1:X2}", PREV, FREV);
            }
        }
    }
}