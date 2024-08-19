import React from 'react';
import Header from './components/Header';
import FilesTable from './components/FilesTable';
import Footer from './components/Footer';
import './App.css';
import logoUTN from './assets/utn.png';

function App() {
  const handleFullClean = () => {
    // This function will be passed to both FilesTable and Footer
    // to trigger a refetch of the file list
    if (window.filesTableRefetch) {
      window.filesTableRefetch();
    }
  };

  return (
    <div className="app">
      <Header
        title="Brújula electrónica: Gestor de archivos"
        imageSrc={logoUTN}
      />
      <main className="main-content">
        <FilesTable onRefetch={handleFullClean} />
      </main>
      <Footer onFullClean={handleFullClean} />
    </div>
  );
}

export default App;