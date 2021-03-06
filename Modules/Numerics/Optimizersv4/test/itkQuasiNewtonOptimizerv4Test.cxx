/*=========================================================================
 *
 *  Copyright Insight Software Consortium
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0.txt
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *=========================================================================*/
#include "itkQuasiNewtonOptimizerv4.h"
#include "itkMeanSquaresImageToImageMetricv4.h"
#include "itkRegistrationParameterScalesFromPhysicalShift.h"
#include "itkRegistrationParameterScalesFromIndexShift.h"
#include "itkRegistrationParameterScalesFromJacobian.h"

#include "itkImageRegistrationMethodImageSource.h"

#include "itkIdentityTransform.h"
#include "itkAffineTransform.h"
#include "itkTranslationTransform.h"

/**
 *  This is a test using GradientDescentOptimizerv4 and parameter scales
 *  estimator. The scales are estimated before the first iteration by
 *  RegistrationParameterScalesFromShift. The learning rates are estimated
 *  at each iteration according to the shift of each step.
 */

template< typename TMovingTransform >
int itkQuasiNewtonOptimizerv4TestTemplated(int numberOfIterations,
                                                          double shiftOfStep,
                                                          std::string scalesOption,
                                                          bool usePhysicalSpaceForShift = true)
{
  const unsigned int Dimension = TMovingTransform::SpaceDimension;
  typedef double PixelType;

  // Fixed Image Type
  typedef itk::Image<PixelType,Dimension>               FixedImageType;

  // Moving Image Type
  typedef itk::Image<PixelType,Dimension>               MovingImageType;

  // Size Type
  typedef typename MovingImageType::SizeType            SizeType;

  // ImageSource
  typedef typename itk::testhelper::ImageRegistrationMethodImageSource<
                                  typename FixedImageType::PixelType,
                                  typename MovingImageType::PixelType,
                                  Dimension >         ImageSourceType;

  typename FixedImageType::ConstPointer    fixedImage;
  typename MovingImageType::ConstPointer   movingImage;
  typename ImageSourceType::Pointer        imageSource;

  imageSource   = ImageSourceType::New();

  SizeType size;
  size[0] = 100;
  size[1] = 100;

  imageSource->GenerateImages( size );

  fixedImage    = imageSource->GetFixedImage();
  movingImage   = imageSource->GetMovingImage();

  // Transform for the moving image
  typedef TMovingTransform MovingTransformType;
  typename MovingTransformType::Pointer movingTransform = MovingTransformType::New();
  movingTransform->SetIdentity();

  // Transform for the fixed image
  typedef itk::IdentityTransform<double, Dimension> FixedTransformType;
  typename FixedTransformType::Pointer fixedTransform = FixedTransformType::New();
  fixedTransform->SetIdentity();

  // ParametersType for the moving transform
  typedef typename MovingTransformType::ParametersType ParametersType;

  // Metric
  typedef itk::MeanSquaresImageToImageMetricv4
    < FixedImageType, MovingImageType, FixedImageType > MetricType;
  typename MetricType::Pointer metric = MetricType::New();

  // Assign images and transforms to the metric.
  metric->SetFixedImage( fixedImage );
  metric->SetMovingImage( movingImage );
  metric->SetVirtualDomainFromImage( const_cast<FixedImageType *>(fixedImage.GetPointer()) );

  metric->SetFixedTransform( fixedTransform );
  metric->SetMovingTransform( movingTransform );

  // Initialize the metric to prepare for use
  metric->Initialize();

  // Optimizer
  typedef itk::QuasiNewtonOptimizerv4  OptimizerType;
  OptimizerType::Pointer optimizer = OptimizerType::New();

  optimizer->SetMetric( metric );
  optimizer->SetNumberOfIterations( numberOfIterations );

  // Instantiate an Observer to report the progress of the Optimization
  typedef itk::CommandIterationUpdate< OptimizerType >  CommandIterationType;
  CommandIterationType::Pointer iterationCommand = CommandIterationType::New();
  iterationCommand->SetOptimizer( optimizer.GetPointer() );

  // Optimizer parameter scales estimator
  typename itk::OptimizerParameterScalesEstimator::Pointer scalesEstimator;

  typedef itk::RegistrationParameterScalesFromPhysicalShift< MetricType > PhysicalShiftScalesEstimatorType;
  typedef itk::RegistrationParameterScalesFromIndexShift< MetricType > IndexShiftScalesEstimatorType;
  typedef itk::RegistrationParameterScalesFromJacobian< MetricType > JacobianScalesEstimatorType;

  if (scalesOption.compare("shift") == 0)
    {
    if( usePhysicalSpaceForShift )
      {
      std::cout << "Testing RegistrationParameterScalesFrom*Physical*Shift" << std::endl;
      typename PhysicalShiftScalesEstimatorType::Pointer shiftScalesEstimator = PhysicalShiftScalesEstimatorType::New();
      shiftScalesEstimator->SetMetric(metric);
      shiftScalesEstimator->SetTransformForward(true); //default
      scalesEstimator = shiftScalesEstimator;
      }
    else
      {
      std::cout << "Testing RegistrationParameterScalesFrom*Index*Shift" << std::endl;
      typename IndexShiftScalesEstimatorType::Pointer shiftScalesEstimator = IndexShiftScalesEstimatorType::New();
      shiftScalesEstimator->SetMetric(metric);
      shiftScalesEstimator->SetTransformForward(true); //default
      scalesEstimator = shiftScalesEstimator;
      }
    }
  else
    {
    std::cout << "Testing RegistrationParameterScalesFromJacobian" << std::endl;
    typename JacobianScalesEstimatorType::Pointer jacobianScalesEstimator
      = JacobianScalesEstimatorType::New();
    jacobianScalesEstimator->SetMetric(metric);
    scalesEstimator = jacobianScalesEstimator;
    }

  optimizer->SetScalesEstimator(scalesEstimator);
  // If SetTrustedStepScale is not called, it will use voxel spacing.
  optimizer->SetMaximumStepSizeInPhysicalUnits(shiftOfStep);
  optimizer->SetMaximumNewtonStepSizeInPhysicalUnits(shiftOfStep*3.0);

  std::cout << "Start optimization..." << std::endl
            << "Number of iterations: " << numberOfIterations << std::endl;

  try
    {
    optimizer->StartOptimization();
    }
  catch( itk::ExceptionObject & e )
    {
    std::cout << "Exception thrown ! " << std::endl;
    std::cout << "An error occurred during Optimization:" << std::endl;
    std::cout << e.GetLocation() << std::endl;
    std::cout << e.GetDescription() << std::endl;
    std::cout << e.what()    << std::endl;
    return EXIT_FAILURE;
    }

  std::cout << "...finished. " << std::endl
            << "StopCondition: " << optimizer->GetStopConditionDescription()
            << std::endl
            << "Metric: NumberOfValidPoints: "
            << metric->GetNumberOfValidPoints()
            << std::endl;

  //
  // results
  //
  ParametersType finalParameters  = movingTransform->GetParameters();
  ParametersType fixedParameters  = movingTransform->GetFixedParameters();
  std::cout << "Estimated scales = " << optimizer->GetScales() << std::endl;
  std::cout << "finalParameters = " << finalParameters << std::endl;
  std::cout << "fixedParameters = " << fixedParameters << std::endl;
  bool pass = true;

  ParametersType actualParameters = imageSource->GetActualParameters();
  std::cout << "actualParameters = " << actualParameters << std::endl;
  const unsigned int numbeOfParameters = actualParameters.Size();

  // We know that for the Affine transform the Translation parameters are at
  // the end of the list of parameters.
  const unsigned int offsetOrder = finalParameters.Size()-actualParameters.Size();

  const double tolerance = 1.0;  // equivalent to 1 pixel.

  for(unsigned int i=0; i<numbeOfParameters; i++)
    {
    // the parameters are negated in order to get the inverse transformation.
    // this only works for comparing translation parameters....
    std::cout << finalParameters[i+offsetOrder] << " == " << -actualParameters[i] << std::endl;
    if( vnl_math_abs ( finalParameters[i+offsetOrder] - (-actualParameters[i]) ) > tolerance )
      {
      std::cout << "Tolerance exceeded at component " << i << std::endl;
      pass = false;
      }
    }

  if( !pass )
    {
    std::cout << "Test FAILED." << std::endl;
    return EXIT_FAILURE;
    }
  else
    {
    std::cout << "Test PASSED." << std::endl;
    return EXIT_SUCCESS;
    }
}

int itkQuasiNewtonOptimizerv4Test(int argc, char ** const argv)
{
  if( argc > 3 )
    {
    std::cerr << "Missing Parameters " << std::endl;
    std::cerr << "Usage: " << argv[0];
    std::cerr << " [numberOfIterations=50 shiftOfStep=1] ";
    std::cerr << std::endl;
    return EXIT_FAILURE;
    }
  unsigned int numberOfIterations = 50;
  double shiftOfStep = 1.0;

  if( argc >= 2 )
    {
    numberOfIterations = atoi( argv[1] );
    }
  if (argc >= 3)
    {
    shiftOfStep = atof( argv[2] );
    }

  const unsigned int Dimension = 2;

  std::cout << std::endl << "Optimizing translation transform with shift scales" << std::endl;
  typedef itk::TranslationTransform<double, Dimension> TranslationTransformType;
  int ret1 = itkQuasiNewtonOptimizerv4TestTemplated<TranslationTransformType>(numberOfIterations, shiftOfStep, "shift");

  std::cout << std::endl << "Optimizing translation transform with Jacobian scales" << std::endl;
  typedef itk::TranslationTransform<double, Dimension> TranslationTransformType;
  int ret2 = itkQuasiNewtonOptimizerv4TestTemplated<TranslationTransformType>(numberOfIterations, shiftOfStep, "jacobian");

  if ( ret1 == EXIT_SUCCESS && ret2 == EXIT_SUCCESS )
    {
    std::cout << std::endl << "Tests PASSED." << std::endl;
    return EXIT_SUCCESS;
    }
  else
    {
    std::cout << std::endl << "Tests FAILED." << std::endl;
    return EXIT_FAILURE;
    }
}
