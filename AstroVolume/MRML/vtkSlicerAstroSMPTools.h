/*==============================================================================

  Copyright (c) Kapteyn Astronomical Institute
  University of Groningen, Groningen, Netherlands. All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

==============================================================================*/

#ifndef __vtkSlicerAstroSMPTools_h
#define __vtkSlicerAstroSMPTools_h

// VTK includes
#include <vtkSMPThreadLocal.h>
#include <vtkSMPTools.h>
#include <vtkType.h>

// STD includes
#include <algorithm>
#include <atomic>
#include <thread>
#include <vector>
#include <limits>

/// \ingroup SlicerAstro_QtModules_AstroVolume
/// Multi-threaded (vtkSMPTools: TBB, STDThread or sequential, as configured
/// in VTK) helpers for the voxel loops of SlicerAstro. Blank voxels (NaN)
/// are ignored by all the reductions.
namespace vtkSlicerAstroSMPTools
{

//----------------------------------------------------------------------------
template <typename T>
inline bool IsNaN(T value)
{
  return value != value;
}

//----------------------------------------------------------------------------
/// Minimum and maximum of data[begin, end), ignoring NaN.
/// minValue/maxValue are left unchanged if there are no valid voxels.
template <typename T>
void Range(const T* data, vtkIdType begin, vtkIdType end, double& minValue, double& maxValue)
{
  vtkSMPThreadLocal<double> localMin(minValue);
  vtkSMPThreadLocal<double> localMax(maxValue);
  vtkSMPTools::For(begin, end, [&](vtkIdType first, vtkIdType last)
    {
    double& threadMin = localMin.Local();
    double& threadMax = localMax.Local();
    for (vtkIdType i = first; i < last; ++i)
      {
      const T value = data[i];
      if (IsNaN(value))
        {
        continue;
        }
      threadMin = std::min(threadMin, static_cast<double>(value));
      threadMax = std::max(threadMax, static_cast<double>(value));
      }
    });
  for (double v : localMin)
    {
    minValue = std::min(minValue, v);
    }
  for (double v : localMax)
    {
    maxValue = std::max(maxValue, v);
    }
}

//----------------------------------------------------------------------------
/// Sum of data[begin, end), ignoring NaN.
template <typename T>
double Sum(const T* data, vtkIdType begin, vtkIdType end)
{
  vtkSMPThreadLocal<double> localSum(0.);
  vtkSMPTools::For(begin, end, [&](vtkIdType first, vtkIdType last)
    {
    double& threadSum = localSum.Local();
    for (vtkIdType i = first; i < last; ++i)
      {
      if (!IsNaN(data[i]))
        {
        threadSum += data[i];
        }
      }
    });
  double sum = 0.;
  for (double v : localSum)
    {
    sum += v;
    }
  return sum;
}

//----------------------------------------------------------------------------
/// Sum of (data[i] - mean)^2 over [begin, end), ignoring NaN.
template <typename T>
double SumSquaredDeviations(const T* data, vtkIdType begin, vtkIdType end, double mean)
{
  vtkSMPThreadLocal<double> localSum(0.);
  vtkSMPTools::For(begin, end, [&](vtkIdType first, vtkIdType last)
    {
    double& threadSum = localSum.Local();
    for (vtkIdType i = first; i < last; ++i)
      {
      if (!IsNaN(data[i]))
        {
        const double d = data[i] - mean;
        threadSum += d * d;
        }
      }
    });
  double sum = 0.;
  for (double v : localSum)
    {
    sum += v;
    }
  return sum;
}

//----------------------------------------------------------------------------
/// Sum of the voxels inside the box extent[6] = {x0,x1,y0,y1,z0,z1}
/// (inclusive) of a volume of dimensions dims[3], ignoring NaN.
/// If squaredDeviationsFrom is not NaN, sums (value - squaredDeviationsFrom)^2
/// instead.
template <typename T>
double ExtentSum(const T* data, const int dims[3], const int extent[6],
                 double squaredDeviationsFrom = std::numeric_limits<double>::quiet_NaN())
{
  const vtkIdType nx = dims[0];
  const vtkIdType nxy = static_cast<vtkIdType>(dims[0]) * dims[1];
  const bool deviations = !IsNaN(squaredDeviationsFrom);
  vtkSMPThreadLocal<double> localSum(0.);
  vtkSMPTools::For(extent[4], extent[5] + 1, [&](vtkIdType zFirst, vtkIdType zLast)
    {
    double& threadSum = localSum.Local();
    for (vtkIdType z = zFirst; z < zLast; ++z)
      {
      for (vtkIdType y = extent[2]; y <= extent[3]; ++y)
        {
        const T* row = data + z * nxy + y * nx;
        for (vtkIdType x = extent[0]; x <= extent[1]; ++x)
          {
          const T value = row[x];
          if (IsNaN(value))
            {
            continue;
            }
          if (deviations)
            {
            const double d = value - squaredDeviationsFrom;
            threadSum += d * d;
            }
          else
            {
            threadSum += value;
            }
          }
        }
      }
    });
  double sum = 0.;
  for (double v : localSum)
    {
    sum += v;
    }
  return sum;
}

//----------------------------------------------------------------------------
/// Parallel loop over [begin, end) with cooperative cancellation: the
/// functor is called on sub-ranges; once \a cancel is set, remaining chunks
/// are skipped. Returns false if the loop was cancelled.
template <typename Functor>
bool ForCancellable(vtkIdType begin, vtkIdType end, const std::atomic<bool>& cancel, Functor&& functor)
{
  vtkSMPTools::For(begin, end, [&](vtkIdType first, vtkIdType last)
    {
    if (cancel.load(std::memory_order_relaxed))
      {
      return;
      }
    functor(first, last);
    });
  return !cancel.load();
}

//----------------------------------------------------------------------------
/// Median of the values (the vector is partially reordered). For an even
/// number of values, the mean of the two central values. NaN if empty.
template <typename T>
double Median(std::vector<T>& values)
{
  const size_t n = values.size();
  if (n == 0)
    {
    return std::numeric_limits<double>::quiet_NaN();
    }
  const size_t half = n / 2;
  std::nth_element(values.begin(), values.begin() + half, values.end());
  const double upper = values[half];
  if (n % 2 == 1)
    {
    return upper;
    }
  // the largest value of the lower half is the other central value
  const double lower = *std::max_element(values.begin(), values.begin() + half);
  return 0.5 * (lower + upper);
}

//----------------------------------------------------------------------------
/// Progress reporting and cancellation for vtkSMPTools loops driven by a
/// SlicerAstro parameters node (any class with int GetStatus()/SetStatus(int);
/// status -1 means "cancel requested").
///
/// The parameters node is only accessed from the thread that created the
/// reporter (the calling thread, which participates in vtkSMPTools::For):
/// SetStatus() fires MRML events that must not be invoked from worker threads.
template <typename ParametersNodeType>
class ProgressReporter
{
public:
  /// Status goes from startStatus to endStatus while processing totalWork items.
  ProgressReporter(ParametersNodeType* pnode, vtkIdType totalWork, int startStatus, int endStatus)
    : Node(pnode)
    , TotalWork(totalWork > 0 ? totalWork : 1)
    , StartStatus(startStatus)
    , EndStatus(endStatus)
    , LastStatus(startStatus)
    , OwnerThread(std::this_thread::get_id())
  {
  }

  /// True once a cancellation has been requested. Cheap: can be called often.
  bool IsCancelled()
  {
    if (std::this_thread::get_id() == this->OwnerThread && this->Node &&
        this->Node->GetStatus() == -1)
      {
      this->Cancelled = true;
      }
    return this->Cancelled.load(std::memory_order_relaxed);
  }

  /// To be called after processing \a work items.
  void Done(vtkIdType work)
  {
    const vtkIdType done = (this->Processed += work);
    if (std::this_thread::get_id() != this->OwnerThread || !this->Node || this->IsCancelled())
      {
      return;
      }
    const int status = this->StartStatus +
      static_cast<int>((this->EndStatus - this->StartStatus) * (static_cast<double>(done) / this->TotalWork));
    if (status >= this->LastStatus + 10)
      {
      this->LastStatus = status;
      this->Node->SetStatus(status);
      }
  }

private:
  ParametersNodeType* Node;
  const vtkIdType TotalWork;
  const int StartStatus;
  const int EndStatus;
  int LastStatus;
  const std::thread::id OwnerThread;
  std::atomic<bool> Cancelled{false};
  std::atomic<vtkIdType> Processed{0};
};

} // namespace vtkSlicerAstroSMPTools

#endif
